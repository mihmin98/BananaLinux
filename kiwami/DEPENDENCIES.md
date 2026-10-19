# The fork's external dependencies

Every piece of outside code the fork builds or ships, with the version it is pinned at and where. A commit that adds a
dependency, or changes its version, updates this file in the same commit.

## The fork's own

| Dependency | Version | Released | Pinned in | Source | Notes |
|---|---|---|---|---|---|
| Tailscale | 1.98.8 | 30 Jun 2026 | `kiwami/package/kiwami-tailscale/kiwami-tailscale.mk` (`KIWAMI_TAILSCALE_VERSION`), the vendored tarball's sha256 in `kiwami-tailscale.hash` | https://github.com/tailscale/tailscale | Its Go modules are vendored through `proxy.golang.org` (one is gone from GitHub) and checked by its `go.sum`. Carries `0001-atomicfile-paths-carry-on-when-the-filesystem-refuses-chmod.patch` (its state on the exFAT card), which a bump checks still applies and is still needed. A newer Tailscale needs a newer Go than Buildroot's (its `go.mod`'s `go` line); a bump recomputes the hash |

## Upstream's, that the fork depends on

Pinned by upstream: they move when `main` is merged into `kiwami`. When one moves, check the fork's packages and
fragments against it (the merge's checks in `docs/FORK.md`).

| Dependency | Version | Pinned in | What of the fork's depends on it |
|---|---|---|---|
| Buildroot | 2026.02.3 | `Makefile` (`BUILDROOT_VERSION`) and CI's clone in `.github/workflows/ci.yml` | `kiwami/package/` (Buildroot's package infrastructures), `kiwami/configs/kiwami.fragment` (option names) |
| Go (Buildroot's `host-go`) | 1.26.3, the prebuilt variant (`BR2_PACKAGE_HOST_GO_BIN`, set in `kiwami/configs/kiwami.fragment`) | Buildroot's `package/go/go.mk` | Go packages in `kiwami/package/` (`kiwami-tailscale`): a module whose `go.mod` asks for a newer Go doesn't build |
| Linux | 7.2.7 | `buildroot/configs/banana_*_defconfig` (`BR2_LINUX_KERNEL_CUSTOM_TARBALL_LOCATION`) | `kiwami/linux.fragment` (option names) |
