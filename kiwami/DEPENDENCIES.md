# The fork's external dependencies

Every piece of outside code the fork builds or ships, with the version it is pinned at and where. A commit that adds a
dependency, or changes its version, updates this file in the same commit.

## The fork's own

| Dependency | Version | Released | Pinned in | Source | Notes |
|---|---|---|---|---|---|
| (none yet) | | | | | |

## Upstream's, that the fork depends on

Pinned by upstream: they move when `main` is merged into `kiwami`. When one moves, check the fork's packages and
fragments against it (the merge's checks in `docs/FORK.md`).

| Dependency | Version | Pinned in | What of the fork's depends on it |
|---|---|---|---|
| Buildroot | 2026.02.3 | `Makefile` (`BUILDROOT_VERSION`) and CI's clone in `.github/workflows/ci.yml` | `kiwami/package/` (Buildroot's package infrastructures), `kiwami/configs/kiwami.fragment` (option names) |
| Go (Buildroot's `host-go`) | 1.26.3 | Buildroot's `package/go/go.mk` | Go packages in `kiwami/package/`: a module whose `go.mod` asks for a newer Go doesn't build |
| Linux | 7.2.7 | `buildroot/configs/banana_*_defconfig` (`BR2_LINUX_KERNEL_CUSTOM_TARBALL_LOCATION`) | `kiwami/linux.fragment` (option names) |
