# 0001 Tailscale: plan

Issue: #1 · Branch: `k/0001-tailscale` · Investigation: [investigation.md](investigation.md) · Status: approved

## Summary

Settings > Network gets a `Tailscale` row whose value is the state (`Off`, `Starting`, `Log in`, `Waiting for
approval`, `Waiting for Wi-Fi`, or this device's address). A on it opens Tailscale's submenu: switch it on or off,
the tailnet's devices, the device's name, log out. Logging in shows the login link and a QR code of it on the top
panel, or uses an auth key put in `banana/tailscale/authkey` on the card. Once on, Tailscale comes back by itself at
every boot.

The approach chosen in the investigation: Tailscale 1.98.8 built from source with Buildroot's Go 1.26.3 (prebuilt
compiler), one `tailscaled` binary with the CLI built in, kernel mode with `CONFIG_TUN=m` and netfilter left off
(`--netfilter-mode=off`, `--accept-dns=false`, `--no-logs-no-support`), its state in `/data/banana/tailscale/`. A
shell helper of the fork's (`/usr/libexec/kiwami/tailscale`) starts and stops it, from the menu and from `rcS`/`rcK`.
banana-ui reads `tailscale status --json` through the helper, only while the Tailscale pages are open (once when
Settings opens, for the row), and draws the QR with Nayuki's qrcodegen, vendored. Off by default: nothing runs and
the module isn't loaded until Tailscale is switched on.

### Refinements over the investigation

Found while working out the exact changes; none changes what the feature does.

- **The fork's class is a nested struct, not a friend.** `Settings` already declares `struct RaWatch;` and keeps a
  `std::shared_ptr<RaWatch>`; the fork's `struct Kiwami;` sits the same way, in one line. A nested class has access
  to `Settings`' private members (the `Item` list, the drawing helpers), so no `friend` is needed, and
  `shared_ptr` works with the type incomplete in `main.cpp`.
- **One more value in an enum: `Page`.** The fork's pages need a `page` value of their own (`KIWAMI`) so that the
  dispatch is `if (page == KIWAMI) ...` and the keyboard can come back to them (`startEntry`'s `back` is a `Page`).
  The `Entry` value is `ENTRY_KIWAMI`. Both are generic: a later feature's pages reuse the same hooks, the fork's
  code telling them apart.
- **Two more hook lines**: `#include "kiwami/kiwami.h"` in `settings.cpp`, and a second line in `ui/Makefile` making
  the fork's headers prerequisites of `banana-ui` (otherwise `make ui` doesn't rebuild after a header change).
  13 hooks in banana-ui in all (*Hooks*, below).
- **The helper's `status`** prints `state off` when tailscaled isn't running (and the menu doesn't even run the
  helper while the setting is `off`), an `error <text>` line for the last failure it knows (no `tun` module, a
  refused auth key), then tailscaled's JSON. The clock problem of oversight 6 comes from the JSON's `Health`
  messages, which tailscaled fills itself.
- **The name is checked when the keyboard's Done is pressed**, not per key: spaces become hyphens, capitals
  lowercase, and anything else left over is refused with a message saying what a name may hold. This keeps the
  keyboard upstream's, with no hook in its key handling.
- **The default name is worked out twice**: by the helper (boot has no menu) and by the menu (to show it while
  Tailscale is off), each from `/proc/device-tree/model` (`banana` when there is none, as in the headless tests).
  Both are a few lines; the helper's test and the menu's test check they agree.
- **Logging out logs in again**: after `tailscale logout` the helper starts a new login at once (a fresh QR, or the
  auth key on the card), as oversight 13 has it. To leave the tailnet for good, switch Tailscale off.
- **With the fork's options off, the renders are `main`'s except the Network list**: its one extra row (`01 / 04`
  instead of `01 / 03`). The investigation decided the row is always there; the final check compares every other
  render.

### Decided: the auth key and File sharing

The investigation had the auth key both written over File sharing and kept out of it with `veto files`; the two
can't both hold, since ksmbd's veto refuses creating a file as it refuses opening one. Decided: **only the state
file is vetoed** (`tailscaled.state`). The key can be dropped into `banana/tailscale/` over File sharing (or written
with the card in a computer); it is readable on the network until the next login uses and deletes it (at once if
Tailscale is on and needs a login, at the next switch-on otherwise). `authkey.refused` is readable too: a refused
key is useless to anyone. Vetoing the key as well would have kept it off the network but made File sharing unable
to write it.

## Steps

Each step is one commit, and leaves the tree building and `make test` passing.

### 1. Tailscale in the image

- **Files**: `kiwami/package/kiwami-tailscale/Config.in`, `kiwami-tailscale.mk`, `kiwami-tailscale.hash` (new);
  `kiwami/Config.in` (sources it); `kiwami/configs/kiwami.fragment`; `kiwami/linux.fragment`;
  `kiwami/DEPENDENCIES.md`.
- **Change**:
  - The package: `golang-package`, `KIWAMI_TAILSCALE_VERSION = 1.98.8`, `SITE = $(call github,tailscale,tailscale,v...)`,
    `GOMOD = tailscale.com`, `BUILD_TARGETS = cmd/tailscaled`, `TAGS = ts_include_cli` plus the `ts_omit_*` tags of
    the features left out (from 1.98.8's own list, `cmd/featuretags`: AWS, Kubernetes, BIRD, TAP, Tailscale SSH,
    Taildrop, Serve/Funnel, Drive, the web client, the system tray, capture, Wake-on-LAN, TPM, the relay server,
    and the rest that the investigation leaves out; kept: the port mapper, DERP, netcheck, the CLI). Each tag
    listed with a word on what it drops. The same version `LDFLAGS` as Buildroot's own package. A post-install
    hook makes `/usr/bin/tailscale` a symlink to `tailscaled` (the CLI by its name). `Config.in`:
    `BR2_PACKAGE_KIWAMI_TAILSCALE`, depends on `BR2_PACKAGE_HOST_GO_TARGET_ARCH_SUPPORTS`, selects
    `BR2_PACKAGE_HOST_GO`, and a comment that Buildroot's `tailscale` package is not to be used (it switches netfilter
    on in the kernel). The `.hash` is "locally computed" over the vendored tarball, as Buildroot's.
  - `kiwami.fragment`: `BR2_PACKAGE_KIWAMI_TAILSCALE=y`, `BR2_PACKAGE_HOST_GO_BIN=y` (Go's prebuilt 1.26.3 rather
    than five bootstrap stages from source).
  - `linux.fragment`: `CONFIG_TUN=m`, with why (kernel mode; loaded only when Tailscale starts; netfilter and
    WireGuard stay off as upstream has them).
  - `DEPENDENCIES.md`: Tailscale 1.98.8 (released 30 Jun 2026, pinned in `kiwami-tailscale.mk`), with the note that a
    newer one needs a newer Go than Buildroot's; and Go's prebuilt variant noted in the upstream table's Go row.
- **Tests**: `kiwami/tests/ci.sh .buildroot` (every board configures with the package and the prebuilt Go, nothing
  dropped). `make kiwami-tailscale-rgds` and a full `make rgds`: `output/rgds/target/usr/bin/tailscaled` is aarch64
  and stripped, its size (and the squashfs's growth) written in this plan's notes; `tun.ko` is in the target's
  modules; the kernel's `.config` has `CONFIG_TUN=m` and still no `CONFIG_NETFILTER`.
- **Commit**: `tailscale: Tailscale 1.98.8 in the image, built from source, with TUN as a kernel module`

### 2. The helper, at boot and shutdown

- **Files**: `kiwami/rootfs/usr/libexec/kiwami/tailscale` (new); `system/rootfs/etc/init.d/rcS` (hook);
  `system/rootfs/etc/init.d/rcK` (hook); `kiwami/post-build.sh` (changed); `kiwami/tests/tailscale-helper.sh`,
  `kiwami/tests/bin/` (stand-in `tailscaled`, `tailscale`, `modprobe`, `logger`) (new); `kiwami/tests/ci.sh`
  (changed); `docs/FORK.md` (*What the fork shadows*: the share script).
- **Change**:
  - The helper, `tailscale boot | up | down | logout | name <name> | status`, in the style of upstream's `ssh` and
    `share`. State in `/data/banana/tailscale/`, socket `/run/tailscale/tailscaled.sock` (*Deviations*), pid
    `/run/banana/tailscaled.pid`, its own log `/data/banana/logs/tailscale.log` (a timestamped line per start, stop,
    login, refusal, failure), tailscaled's output to syslog through `logger -t tailscaled`. The data and run
    folders can be moved by environment variables, for the test only.
    - `boot`: `up` when `settings.conf` has `tailscale = on`.
    - `up`: `modprobe tun` (if that fails: logged, `error` line kept, exit 1); tailscaled started if it isn't
      running (`--statedir --socket --tun=tailscale0 --no-logs-no-support`); its socket waited for (a few seconds at
      most); then `tailscale up --accept-dns=false --netfilter-mode=off --hostname=<name>` in the background, with
      `--auth-key=file:.../authkey` when that file is there. When it ends: success deletes the key; a refusal renames
      it `authkey.refused` and keeps the reason as the `error` line.
    - `down`: tailscaled stopped (SIGTERM, waited for; it takes `tailscale0` down itself), `modprobe -r tun`. The
      login stays in the state.
    - `logout`: `tailscale logout`, then a new login as in `up`.
    - `name <name>`: `tailscale set --hostname=<name>` while tailscaled runs (the next `up` passes it anyway).
    - `status`: `state off` when tailscaled isn't running; else the `error` line if there is one, then
      `tailscale status --json`.
    - The name: `tailscale_name` from `settings.conf`, else `banana-rgds` / `banana-rgds-plus` from the device tree's
      model, else `banana`.
  - `rcS`, in the background subshell after `ssh up`:
    `/usr/libexec/kiwami/tailscale boot &	# kiwami: Tailscale, when switched on` (its wait for the socket doesn't
    hold up the panel tuner).
  - `rcK`, after `ssh down`: `/usr/libexec/kiwami/tailscale down 2>/dev/null	# kiwami: before /data is unmounted`.
  - `post-build.sh`: in `$TARGET_DIR/usr/libexec/banana/share`, `*tailscaled.state/` added to the `veto files` line
    (the auth key stays visible: *Decided*, above). The script fails the build if that line isn't found as
    expected, so an upstream change to it stops the build rather than leaving the state shared.
- **Tests**: `kiwami/tests/tailscale-helper.sh`, run by `ci.sh`: the real helper against stand-ins in `PATH` (the
  stand-in `tailscale` succeeds or refuses an auth key as told, and records its arguments): `boot` does nothing with
  the setting off or absent and runs `up` with it on; `up` loads `tun` and passes exactly the flags above; a key is
  deleted after a login, renamed and its reason given by `status` after a refusal; `down` stops the stand-in daemon
  and unloads `tun`; `logout` logs out and starts a login; `name` calls `set`; the default name from a made-up model
  file for each board; `status` prints `state off` when nothing runs. `ci.sh` also runs `post-build.sh` over a copy
  of upstream's `share` and checks the veto line, and its `sh -n` loop covers the helper. In QEMU
  (`tools/qemu-boot.sh --interactive`, user networking): `tailscale up` from the helper reaches `NeedsLogin` with a
  login URL in `status` (the network path works end to end before the menu is touched).
- **Commit**: `tailscale: the helper that starts and stops it, at boot and before /data is unmounted`

### 3. banana-ui: the fork's hooks and the Tailscale row

- **Files**: `ui/Makefile` (hook, 2 lines); `ui/src/settings.h` (hook, 3 lines); `ui/src/settings.cpp` (hook, 10
  lines); `ui/src/kiwami/kiwami.h`, `kiwami.cpp`, `json.h`, `json.cpp`, `tsstatus.h`, `tsstatus.cpp` (new);
  `kiwami/tests/fake-tailscale.sh`, `kiwami/tests/units.cpp`, `kiwami/tests/tailscale/*.json`,
  `kiwami/tests/tailscale-ui.sh` (new); `kiwami/tests/ci.sh` (changed).
- **Change**:
  - The hooks, all of them now (each one line, marked `kiwami:`, listed under *Hooks* below); the later steps add
    only the fork's own files.
  - `Settings::Kiwami` (`kiwami.h`/`.cpp`): the fork's rows and pages behind the hooks. It adds the `Tailscale` row
    (`PAGE`, key `kiwami.tailscale`) to the Network group after `File sharing`, with its help; on Settings opening, it
    reads `tailscale` from `settings.conf` and runs the helper's `status` once (in the background, a `wifi::Job`,
    at once in the headless test) only when it is `on`; the row's value comes from the answer. The helper is
    `/usr/libexec/kiwami/tailscale`, or `KIWAMI_TAILSCALE` in the environment (the test's stand-in); a helper that
    isn't there reads as `Off` and nothing is run. `open()` of the row is a stub page in this step (the submenu
    comes in step 4) so the hook is exercised: B goes back to the list.
  - `json.h`/`.cpp`: a small JSON reader (objects, arrays, strings with `\u` escapes, numbers, booleans, null) into a
    tree; nothing else of banana-ui's uses it.
  - `tsstatus.h`/`.cpp`: the helper's `status` output into a struct: the state (`Off`, `Starting`, `NeedsLogin`,
    `NeedsMachineAuth`, `Running`, `Stopped`), the login URL, this device (name, MagicDNS name, addresses, expired),
    the peers (name, MagicDNS name, addresses, OS, online, last seen), `Health`, the helper's error; and the row's
    word for it. Pure functions, unit-tested.
  - Each change of state is logged on stderr (`tailscale: state Running, 100.64.0.7`), for the tests.
- **Tests**: `kiwami/tests/units.cpp`, built and run by `ci.sh` with the host's `g++`: the JSON reader (escapes,
  nesting, malformed input refused without crashing), `tsstatus` over captured outputs in
  `kiwami/tests/tailscale/` (off, starting, needs login with a URL, needs approval, running with peers, expired,
  refused key, unhealthy clock). The captures are written from tailscaled 1.98.8's JSON fields; manual test M2
  replaces them with real ones from the device. `kiwami/tests/tailscale-ui.sh`, run by `ci.sh` when `ui/banana-ui`
  is built: at 640x480 and 1024x768, the Network list with the stand-in in each state (shots, and the row's value
  in the log), the row's stub page opened and closed; with the setting off the stand-in is never called (its
  record stays empty). `make test` passes unchanged.
- **Commit**: `banana-ui: the fork's rows and pages hooked into Settings, and Tailscale's row in Network`

### 4. The Tailscale submenu

- **Files**: `ui/src/kiwami/tailscale.cpp` (new), `ui/src/kiwami/kiwami.h`/`.cpp` (changed);
  `kiwami/tests/tailscale-ui.sh`, `kiwami/tests/units.cpp` (changed).
- **Change**:
  - The submenu replaces the stub: on the bottom panel, in the Settings list's look (the same `box`/`text`/`arrow`/
    `face` helpers), `Tailscale` ◀ On / Off ▶, `Devices` (when logged in, value: how many are online; opens a
    placeholder until step 6), `Device name` (value: the name), `Log out` (logged in; A twice, as `Restart`). Touch
    on the rows and the B / A chips as upstream's list.
  - On / Off writes `tailscale` in `settings.conf` and runs the helper's `up` or `down` (detached, as upstream's
    `spawn`, so it survives the menu starting a game).
  - `Device name` opens upstream's keyboard (`startEntry(ENTRY_KIWAMI, ..., KIWAMI, <name>)`); Done normalises and
    checks the name (a DNS label: `a-z 0-9 -`, 1-63, no hyphen at either end), writes `tailscale_name`, and runs the
    helper's `name`; a refused name keeps the keyboard open with a message.
  - `Log out` runs the helper's `logout`.
  - The top panel shows the state, whatever row is selected, then that row's help: Off (what Tailscale is, that it
    starts at every boot once on, that SSH and File sharing become reachable from the whole tailnet), Starting,
    Waiting for Wi-Fi (tailscaled running, no address on the handheld and this device not online), Needs login
    (the link as text; the QR comes in step 5), Waiting for approval, Connected (name, address, MagicDNS name,
    `ssh root@<address>`, `\\<address>\banana`), an error (the helper's, or a `Health` message; a certificate or
    time error says to connect Wi-Fi so the clock is set). The name row's help says a rename in the admin console
    stops the handheld's own name from showing there; Log out's says Tailscale logs in again at once.
  - While the submenu is open, the helper's `status` every 2 s; nothing when it's closed (`pageWake` says when).
- **Tests**: `units.cpp`: the name's normalisation and checks. `tailscale-ui.sh`, both sizes: the submenu in each
  state (shots); On / Off writes `settings.conf` and calls the stand-in's `up` / `down`; a name typed by touch (as
  `run.sh` types a password) saved and passed to `name`, a bad one refused; Log out needs two A's and calls
  `logout`; B back to the Network list; polling stops once closed (no stand-in calls after B in its record).
- **Commit**: `banana-ui: Tailscale's submenu: on or off, the device's name, log out, and its state on the top panel`

### 5. The login QR code

- **Files**: `ui/src/kiwami/qrcodegen.hpp`, `qrcodegen.cpp` (new, vendored unchanged, MIT notice kept);
  `ui/src/kiwami/tailscale.cpp` (changed); `kiwami/DEPENDENCIES.md`; `kiwami/tests/units.cpp`,
  `kiwami/tests/tailscale-ui.sh` (changed).
- **Change**:
  - Nayuki's QR Code generator (C++, v1.8.0) in the fork's sources.
  - Needs login: the URL encoded once (error correction M) into a texture of one pixel per module with the quiet
    zone (`Backend::upload`, not smoothed), drawn at a whole number of device pixels per module, as large as the
    top panel allows (8 px at 640x480, 12 at 1024x768), with the link and the instructions beside it, and the auth
    key's path. A new URL makes a new texture; none is kept once the page closes. Once logged in, the top panel
    turns to the next state by itself (the polling of step 4).
  - `DEPENDENCIES.md`: qrcodegen 1.8.0, pinned in `ui/src/kiwami/qrcodegen.*` (vendored), its source.
- **Tests**: `units.cpp`: a 45-character login URL gives a version 4 code (33 modules). `tailscale-ui.sh`: the Needs
  login shots at both sizes; when `zbarimg` is installed, it decodes the top panel's shot back to the stand-in's
  URL (skipped with a note otherwise; CI doesn't install it).
- **Commit**: `banana-ui: the Tailscale login link as a QR code, scanned from a phone`

### 6. The tailnet's devices

- **Files**: `ui/src/kiwami/tailscale.cpp` (changed); `kiwami/tests/tailscale-ui.sh` (changed).
- **Change**: `Devices` opens the devices page: on the bottom panel the devices, this one first then the online ones,
  then by name, each with its name, IPv4 address and online/offline, scrolling as upstream's lists; on the top
  panel the selected one in full: MagicDNS name, IPv4 and IPv6 addresses, OS, and when it was last seen if
  offline. B goes back to the submenu. It polls as the submenu does.
- **Tests**: `tailscale-ui.sh`, both sizes: the devices page with the stand-in's peers (more than five, to scroll),
  the order checked in the log, the selected device's details shot, B back to the submenu.
- **Commit**: `banana-ui: the tailnet's devices, from Tailscale's submenu`

### 7. Final check

- `make test`, `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`, `kiwami/tests/ci.sh .buildroot`.
- The fork's tests in `kiwami/tests/` (`tailscale-helper.sh`, `units.cpp`, `tailscale-ui.sh`, run by `ci.sh`).
- The hooks: `git diff --stat main...HEAD` and `git grep -n 'kiwami:'` (the commands in `docs/FORK.md`) agree, and
  list only the hooks this feature meant to add (*Hooks*, below).
- With the fork's options off, `make test`'s renders in `ui/tests/out/` match `main`'s, except `wifi-row` and
  `share-row` (the Network list's extra row, *Refinements*).
- `kiwami/DEPENDENCIES.md` lists Tailscale 1.98.8, Go's prebuilt variant and qrcodegen 1.8.0.
- `docs/FORK.md` (the share script among the shadowed files, done in step 2), `docs/ARCHITECTURE.md` (nothing of
  upstream's described there changes; checked), `CLAUDE.md` (nothing to add; checked) updated where needed.
- The footprint measured: the binary and squashfs sizes (step 1), tailscaled's memory and CPU (M14), written in the
  investigation's *Risks* in place of the estimates.
- The manual tests below all passed.

### Hooks

Every line this feature adds to upstream's files.

| File | Where | Line |
|---|---|---|
| `ui/Makefile` | after `SRC =` | `SRC += $(wildcard src/kiwami/*.cpp)	# kiwami: the fork's sources` |
| `ui/Makefile` | after it | `banana-ui: $(wildcard src/kiwami/*.h src/kiwami/*.hpp)	# kiwami: and their headers` |
| `ui/src/settings.h` | `enum Page` | `GAME, KIWAMI }`, marked `// kiwami: KIWAMI, the fork's pages` |
| `ui/src/settings.h` | `enum Entry` | `ENTRY_SEARCH, ENTRY_KIWAMI }`, marked `// kiwami: ENTRY_KIWAMI, a text for the fork's pages` |
| `ui/src/settings.h` | beside `struct RaWatch;` | `struct Kiwami; std::shared_ptr<Kiwami> kw;	// kiwami: the fork's rows and pages (ui/src/kiwami/)` |
| `ui/src/settings.cpp` | the includes | `#include "kiwami/kiwami.h"	// kiwami: the fork's rows and pages` |
| `ui/src/settings.cpp` | `build()`, after File sharing | `Kiwami::rows(*this, W);	// kiwami: Tailscale` |
| `ui/src/settings.cpp` | `open()` | `if (kw) kw->opened();	// kiwami: the rows' values, once` |
| `ui/src/settings.cpp` | `change()`, `PAGE` | `if (kw && kw->owns(it.key)) return kw->open(it.key);	// kiwami:` |
| `ui/src/settings.cpp` | `press()` | `if (page == KIWAMI) return kw->press(k);	// kiwami:` |
| `ui/src/settings.cpp` | `touch()` | `if (page == KIWAMI) return kw->touch(cx, cy);	// kiwami:` |
| `ui/src/settings.cpp` | `render()` | `if (page == KIWAMI) { kw->render(); return; }	// kiwami:` |
| `ui/src/settings.cpp` | `render()`, a page row's value | `else if (it.kind == Item::PAGE && kw && kw->owns(it.key)) v = kw->value(it.key);	// kiwami:` |
| `ui/src/settings.cpp` | `update()`, before `return changed` | `if (kw) changed \|= kw->update(t);	// kiwami:` |
| `ui/src/settings.cpp` | `pageWake()`, first | `if (int w = kw ? kw->wake(t, page == KIWAMI) : -1; w >= 0) return w;	// kiwami:` |
| `ui/src/settings.cpp` | `finishEntry()` | `case ENTRY_KIWAMI: return kw->entered(pw);	// kiwami:` |
| `system/rootfs/etc/init.d/rcS` | background subshell, after SSH | `/usr/libexec/kiwami/tailscale boot &	# kiwami: Tailscale, when switched on` |
| `system/rootfs/etc/init.d/rcK` | after `ssh down` | `/usr/libexec/kiwami/tailscale down 2>/dev/null	# kiwami: before /data is unmounted` |

The two enum lines change an upstream line rather than adding one: an upstream page or entry added there is a
conflict, resolved by keeping both values.

## Automated tests

| Test | Where | What it checks |
|---|---|---|
| Configure | `kiwami/tests/ci.sh` | every board configures with the package, the prebuilt Go and `CONFIG_TUN=m`'s fragment, nothing dropped |
| Shell | `kiwami/tests/ci.sh` | the helper parses (`sh -n`) |
| Veto | `kiwami/tests/ci.sh` | `post-build.sh` adds the state file to the share script's `veto files`, and fails if the line moved |
| Helper | `kiwami/tests/tailscale-helper.sh` | boot, up (flags, `tun`), down, logout, name, status; the auth key deleted after a login, renamed after a refusal; the default name per board |
| Units | `kiwami/tests/units.cpp` | the JSON reader, the status parser over captured outputs, the name's checks, the QR's version for a login URL |
| Menu | `kiwami/tests/tailscale-ui.sh` | at both sizes: the row in each state, the submenu, on/off and the settings written, the name typed, log out, the QR (decoded with `zbarimg` when installed), the devices page; nothing polled when closed or off |
| Upstream | `make test` | unchanged and passing; renders as `main`'s but for the Network list |

## Manual tests

On the device, for what headless tests can't reach. Logs are fetched over SSH/SFTP (Settings > System > SSH) or from
the card on a computer (`banana/logs/` on the card is `/data/banana/logs/` on the device). Commands are run over
SSH; `/run/banana/messages` is syslog (tailscaled's own output), in RAM: copy it before a restart.

| # | Board | Steps | Expected | Logs to collect | Outcome |
|---|---|---|---|---|---|
| M1 | rgds, rgds-plus | Fresh card, Tailscale never switched on. `ps`, `lsmod`, `ip link` | No tailscaled, no `tun` module, no `tailscale0`; the row says `Off` | `ps` / `lsmod` output | not run |
| M2 | rgds, rgds-plus | Switch Tailscale on; scan the QR with a phone; log in and approve | `Starting`, then the QR (scans first time on each panel size); the page turns to Connected by itself; the row shows the address | `/data/banana/logs/tailscale.log`, `/run/banana/messages`, `tailscale status --json` saved into `kiwami/tests/tailscale/` for the unit tests | not run |
| M3 | rgds | Tailnet with device approval on (admin console); log in | `Waiting for approval` until approved in the console, then Connected | `tailscale.log`, `messages` | not run |
| M4 | rgds-plus | Logged out; an auth key in `banana/tailscale/authkey`, dropped in over File sharing; switch on. Then a revoked key | Logged in with no QR, the key file gone. The revoked one: `authkey.refused`, the reason on the page, the QR shown | `tailscale.log` | not run |
| M5 | rgds, rgds-plus | Connected; restart; then shut down and power on | Connected again by itself each time, no login; time from the menu's first frame to Connected noted | `tailscale.log`, `logs/boot.txt`, `messages` | not run |
| M6 | rgds | Switch off; restart | tailscaled gone, `tun` unloaded, no `tailscale0`; still off after the restart, nothing started | `tailscale.log`, `lsmod` | not run |
| M7 | rgds, rgds-plus | Connected; sleep 1 min, wake; sleep 15 min, wake; ping the handheld's 100.x from another device right after each wake | Reachable again within ~10 s each time (the RG DS's Wi-Fi unloaded and loaded around the sleep); the time noted | `messages` (tailscaled's link-change lines), `tailscale.log`, `logs/` bananad's | not run |
| M8 | rgds | Connected; Wi-Fi off in Settings; then on | Row and page say `Waiting for Wi-Fi`; Connected again once Wi-Fi is back | `messages` | not run |
| M9 | rgds-plus | From a phone on mobile data (not the handheld's network): `ssh root@100.x`; `\\100.x\banana` (or `smb://`) | Both work. Over SMB, `banana/tailscale/tailscaled.state` is neither listed nor opened; an `authkey` put there is listed until a login deletes it | `ls` of the share from the computer | not run |
| M10 | rgds | Over SSH on the handheld: `ping -c3` a tailnet device's 100.x; `wget` a service at home by its 100.x | Both answer (the handheld reaches out through `tailscale0`) | the commands' output | not run |
| M11 | rgds | Rename to `my handheld` while connected; then a name with `_` | `my-handheld` in the admin console within a minute; the `_` one refused with the message | `tailscale.log` | not run |
| M12 | rgds-plus | Log out (A twice) | The admin console shows the device logged out; a fresh QR at once | `tailscale.log` | not run |
| M13 | rgds | Connected, game running over SSH: `poweroff` | tailscaled stopped before the unmount; the next boot's `dmesg` has no exFAT "not properly unmounted" | `logs/dmesg.txt`, `logs/boot.txt`, `tailscale.log` | not run |
| M14 | rgds, rgds-plus | Connected and idle in the menu 10 min; then the same with Tailscale off. Then 30 min each, battery % before and after | tailscaled's `VmRSS` (`/proc/<pid>/status`) and CPU (`utime+stime` from `/proc/<pid>/stat` at the start and end); battery drop with it on vs off; written in *Notes* | the numbers | not run |
| M15 | rgds | Connected; play a 3D DS game (SuperDrastic) and a RetroArch game 10 min each | No stutter or slowdown against Tailscale off; tailscaled's CPU over the game | `/proc/<pid>/stat` before/after | not run |
| M16 | rgds | Connected; apply an A/B update package (Settings > System > Update), restart | Still logged in after the update (state on `/data`) | `tailscale.log` | not run |
| M17 | rgds | Over SSH: clock set back a year (`date -s`), `tailscale down`/`up` from the helper, open the page | The page says the clock is wrong and to connect Wi-Fi; after NTP (Wi-Fi reconnected) it connects | `messages`, `tailscale status --json` | not run |
| M18 | rgds, rgds-plus | A `universal` card (built locally), Tailscale on with no name set, on each handheld | `banana-rgds` on the RG DS, `banana-rgds-plus` on the Plus | `tailscale.log` | not run |

Notes from the runs (what the logs showed, what was fixed):

- **Step 1, the build** (2026-10-19, on `output/rgds-plus`: the same arch and kernel as the RG DS's, and already
  built, so the full build was incremental). `kiwami-tailscale` builds in 17 s; `tailscaled` is aarch64, linked
  against glibc (Buildroot turns cgo on when the toolchain can, as for its own package), stripped: 15.7 MB, against
  39.4 MB built with the CLI alone and nothing omitted (gzipped: 6.0 MB against 13.3 MB). The squashfs grows by
  7.26 MB (161.97 to 169.23 MB, lz4 HC), the `.img.gz` by 6.2 MB, the update package by 7.25 MB. `tun.ko` (77 KB) is
  in the target's modules; the kernel's `.config` has `CONFIG_TUN=m`, and `CONFIG_NETFILTER` and `CONFIG_WIREGUARD`
  still unset. Under `qemu-aarch64-static`, `tailscale version` (through the symlink) says 1.98.8 / go1.26.3, and
  every flag the helper uses is there (`up --accept-dns --netfilter-mode --auth-key --hostname`, `set --hostname`,
  `logout`, tailscaled's `--statedir --socket --tun --no-logs-no-support`). The vendored tarball comes out
  byte-identical from a second download, so the `.hash` holds.
- **Step 2, in QEMU** (2026-10-19, `rgds-plus` image, `tools/qemu-boot.sh`'s card, then the helper driven over SSH).
  `qemu-boot.sh`: 48 passed, 0 failed (the menu at 4.2 s, as before). Before Tailscale is first switched on: no
  tailscaled, no `tun`, no `tailscale0`, `status` says `state off`. `up` returns in 1.1 s; 15 s later `status` gives
  `NeedsLogin` with an `AuthURL` from login.tailscale.com (QEMU's user networking), and a plain `tailscale status`
  over SSH answers (the default socket). `tailscaled.state` (119 bytes, with its machine key) is written in
  `banana/tailscale/` on the exFAT `/data` (`fmask=0000`, no `quiet`): the patch works. `tailscale0` up, `tun`
  loaded. Resident memory under QEMU: tailscaled 25 MB; the `tailscale up` waiting for a login 15 MB more, while
  it waits (M14 measures it on the device). `down` in 1.1 s: no tailscaled, `tun` unloaded, no `tailscale0`, the
  state kept. With `tailscale = on`, `poweroff`: rcK stopped it (`stopped` in `tailscale.log`) before the unmount;
  the next boot started it from rcS with the same state file (`NeedsLogin`, a new login URL), and no exFAT
  warnings in `dmesg`. syslog got 110 lines of tailscaled's in the first 15 s, most at start.

## Deviations

Where the implementation turned out different from this plan, why, and what changed (dated).

- **2026-10-19, step 1: vendored through Go's module proxy.** Buildroot vendors a Go package with
  `GOPROXY=direct`, each module from its own repository, and one of Tailscale's (indirect) dependencies,
  `github.com/kenshaw/evdev` v0.1.0, is no longer on GitHub, so the download failed. `proxy.golang.org` still has it:
  the package sets `KIWAMI_TAILSCALE_GO_ENV = GOPROXY=https://proxy.golang.org,direct`, which comes after
  Buildroot's in the download's environment. Nothing is trusted more than before: `go mod vendor` checks every
  module against Tailscale's `go.sum`, and the build stays offline (`-mod=vendor`).
- **2026-10-19, step 1: more features left out than the plan named.** Beyond the plan's list and what the
  investigation leaves out, `ts_omit_*` also drops userspace networking (gVisor's netstack, and GRO and the outbound
  proxy, which need it: kernel mode is the only mode, so a missing `tun` is an error, as step 2 has it), the
  integrations this OS has nothing for (iptables, systemd, D-Bus, NetworkManager, resolved, the desktop, a browser,
  cloud and Synology detection), log upload (`--no-logs-no-support` anyway), Tailscale's self-update, MDM and posture,
  OAuth and federated auth keys, what polls for something nobody looks at here (listening ports, the tun's counters
  and link speed, user metrics), and the CLI's terminal extras (its QR, colours, shell completion). Kept, besides the
  port mapper, DERP, netcheck and the CLI: osrouter (kernel mode's addresses and routes), the IPN bus (`tailscale up`
  waits on it, which the helper needs to tell a login from a refused key), health, DNS (for a later on-device
  MagicDNS), Tailnet Lock (a tailnet using it would otherwise refuse the handheld), c2n, the peer API, the baked-in CA
  roots, captive portal detection, debug and doctor. Each group is commented in `kiwami-tailscale.mk`.
- **2026-10-19, step 2: a patch so tailscaled can keep its state on exFAT.** tailscaled `chmod`s its state folder to
  0700 when it is called `tailscale` (`paths.MkStateDir`, on start) and every state file it writes to 0600
  (`atomicfile.WriteFile`), and fails when that fails. `/data` is exFAT mounted with `fmask=0000,dmask=0000` and
  without `quiet`, and exFAT refuses a `chmod` taking bits away with `EPERM` (`exfat_sanitize_mode`): tailscaled would
  exit at start, and could never save a login. Decided (over the `quiet` mount option, which changes upstream's
  behaviour for all of `/data`, and the state in RAM copied to the card, which loses key rotations to a crash): step
  2 adds `kiwami/package/kiwami-tailscale/0001-*.patch`, by which both carry on when the `chmod` is refused with
  `EPERM`. Nothing else of tailscaled's `chmod`s or `chown`s under its state folder (checked in 1.98.8's source:
  the rest is the socket in `/run`, `/etc/resolv.conf` which `--accept-dns=false` leaves alone, Synology, and the
  SSH server left out). A Tailscale bump carries the patch (`DEPENDENCIES.md` says so).
- **2026-10-19, step 2: tailscaled's socket where Tailscale puts it**, `/run/tailscale/tailscaled.sock` (`/var/run`
  is `/run`), rather than `/run/banana/tailscaled.sock`: `tailscale ...` over SSH then finds the daemon with no
  `--socket` (the manual tests use it). The pid file stays `/run/banana/tailscaled.pid`.
- **2026-10-19, step 2: `ts_omit_unixsocketidentity` taken out of step 1's tags.** In QEMU every CLI call was
  refused (`Access denied: status access denied`): with that tag, 1.98.8's `GetConnIdentity` never marks a
  connection as the unix socket's, so `actor.Permissions` grants neither read nor write, whatever the feature's
  description says. The feature is kept (65 KB); everything here runs as root, so it changes nothing else.

## Progress

A step is marked done in its own commit, so the commit column holds the subject (a commit can't carry its own hash).

| Step | Status | Commit |
|---|---|---|
| 1. Tailscale in the image | done | `tailscale: Tailscale 1.98.8 in the image, built from source, with TUN as a kernel module` |
| 2. The helper, at boot and shutdown | done | `tailscale: the helper that starts and stops it, at boot and before /data is unmounted` |
| 3. banana-ui: the fork's hooks and the Tailscale row | todo | |
| 4. The Tailscale submenu | todo | |
| 5. The login QR code | todo | |
| 6. The tailnet's devices | todo | |
| 7. Final check | todo | |
