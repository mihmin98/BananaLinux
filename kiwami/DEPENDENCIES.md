# The fork's external dependencies

Every piece of outside code the fork builds or ships, with the version it is pinned at and where. A commit that adds a
dependency, or changes its version, updates this file in the same commit.

## The fork's own

| Dependency | Version | Released | Pinned in | Source | Notes |
|---|---|---|---|---|---|
| Tailscale | 1.98.8 | 30 Jun 2026 | `kiwami/package/kiwami-tailscale/kiwami-tailscale.mk` (`KIWAMI_TAILSCALE_VERSION`), the vendored tarball's sha256 in `kiwami-tailscale.hash` | https://github.com/tailscale/tailscale | Its Go modules are vendored through `proxy.golang.org` (one is gone from GitHub) and checked by its `go.sum`. Carries `0001-atomicfile-paths-carry-on-when-the-filesystem-refuses-chmod.patch` (its state on the exFAT card), which a bump checks still applies and is still needed. A newer Tailscale needs a newer Go than Buildroot's (its `go.mod`'s `go` line); a bump recomputes the hash |
| QR Code generator (Nayuki's, C++) | 1.8.0 | 17 Apr 2022 | `ui/src/kiwami/qrcodegen.cpp`, `qrcodegen.hpp` (vendored unchanged from the tag's `cpp/`, commit `720f62bd`; sha256 `1f3b3fcd...` and `b779c3b1...`) | https://github.com/nayuki/QR-Code-generator | MIT, its notice kept in both files. Draws Tailscale's login link as a QR code in banana-ui. A bump copies the two files of the new tag over, unchanged |
| Mesen (libretro) | commit `0102910` | 20 Apr 2026 | `kiwami/package/kiwami-libretro-mesen/kiwami-libretro-mesen.mk` (`KIWAMI_LIBRETRO_MESEN_VERSION`), the tarball's and `LICENSE`'s sha256 in `kiwami-libretro-mesen.hash` | https://github.com/libretro/Mesen | GPL-3.0. The NES's second core (`mesen_libretro.so`, its saves in `/data/saves/Mesen/`). The repository is in maintenance (Mesen 2 has no libretro core); ROCKNIX ships the same commit. A bump recomputes the hash |
| SameBoy (libretro) | commit `aa158a8` | 23 Jul 2026 | `kiwami/package/kiwami-libretro-sameboy/kiwami-libretro-sameboy.mk` (`KIWAMI_LIBRETRO_SAMEBOY_VERSION`), the tarball's and `LICENSE`'s sha256 in `kiwami-libretro-sameboy.hash` | https://github.com/libretro/SameBoy | MIT. The Game Boy's and Game Boy Color's second core (`sameboy_libretro.so`, its saves in `/data/saves/SameBoy/`), with its own open boot ROMs. They are turned into C with `od` in a pre-build hook rather than the makefile's `hexdump`: a bump checks that `libretro/Makefile`'s `%_boot.c` rule and `libretro/Makefile.common`'s list of them still match the hook. `GIT_VERSION` is passed (its own commit, not this repository's). ROCKNIX ships the same commit. A bump recomputes the hash |

## Upstream's, that the fork depends on

Pinned by upstream: they move when `main` is merged into `kiwami`. When one moves, check the fork's packages and
fragments against it (the merge's checks in `docs/FORK.md`).

| Dependency | Version | Pinned in | What of the fork's depends on it |
|---|---|---|---|
| Buildroot | 2026.02.3 | `Makefile` (`BUILDROOT_VERSION`) and CI's clone in `.github/workflows/ci.yml` | `kiwami/package/` (Buildroot's package infrastructures), `kiwami/configs/kiwami.fragment` (option names) |
| Go (Buildroot's `host-go`) | 1.26.3, the prebuilt variant (`BR2_PACKAGE_HOST_GO_BIN`, set in `kiwami/configs/kiwami.fragment`) | Buildroot's `package/go/go.mk` | Go packages in `kiwami/package/` (`kiwami-tailscale`): a module whose `go.mod` asks for a newer Go doesn't build |
| RetroArch | v1.22.2, built with its 7z support (`HAVE_7ZIP`, on by default; the `.mk` doesn't pass `--disable-7zip`) | `buildroot/package/retroarch/retroarch.mk` (`RETROARCH_VERSION`, `RETROARCH_CONF_OPTS`) | The 7z ROMs (`kiwami/post-build.sh` lists `.7z` games on seven systems): built without 7z, they'd be listed and fail to start |
| Linux | 7.2.7 | `buildroot/configs/banana_*_defconfig` (`BR2_LINUX_KERNEL_CUSTOM_TARBALL_LOCATION`) | `kiwami/linux.fragment` (option names) |
