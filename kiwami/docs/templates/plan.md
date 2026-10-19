# <NNNN> <Feature>: plan

Issue: #<N> · Branch: `k/<NNNN>-<name>` · Investigation: [investigation.md](investigation.md) · Status: draft | approved

## Summary

What the feature does once done, and the approach chosen in the investigation, in a few lines.

## Steps

Each step is one commit, and leaves the tree building and `make test` passing.

### 1. <Step>

- **Files**: `path` (new | hook | changed)
- **Change**: exactly what is added or changed, and why.
- **Tests**: the automated tests added or run for this step.
- **Commit**: the subject line it will have.

### N. Final check

- `make test`, `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`, `kiwami/tests/ci.sh .buildroot`.
- The fork's tests in `kiwami/tests/`.
- The hooks: `git diff --stat main...HEAD` and `git grep -n 'kiwami:'` (the commands in `docs/FORK.md`) agree, and
  list only the hooks this feature meant to add.
- With the fork's options off, `make test`'s renders in `ui/tests/out/` match `main`'s.
- `kiwami/DEPENDENCIES.md` lists every dependency the feature adds or bumps, with its version.
- `docs/FORK.md`, `docs/ARCHITECTURE.md`, `CLAUDE.md` updated where the feature changes what they describe.
- The manual tests below all passed.

## Automated tests

| Test | Where | What it checks |
|---|---|---|
| | `kiwami/tests/...` | |

## Manual tests

On the device, for what headless tests can't reach. Logs are fetched over SSH/SFTP (Settings > System > SSH) or from
the card on a computer (`banana/logs/` on the card is `/data/banana/logs/` on the device).

| # | Board | Steps | Expected | Logs to collect | Outcome |
|---|---|---|---|---|---|
| M1 | rgds / rgds-plus | | | `/data/banana/logs/...` | not run |

Notes from the runs (what the logs showed, what was fixed):

## Deviations

Where the implementation turned out different from this plan, why, and what changed (dated).

## Progress

A step is marked done in its own commit, so the commit column holds the subject (a commit can't carry its own hash).

| Step | Status | Commit |
|---|---|---|
| 1. <Step> | todo | |
| N. Final check | todo | |
