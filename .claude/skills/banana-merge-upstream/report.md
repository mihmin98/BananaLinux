# Merge of main into kiwami: <YYYY-MM-DD>

main: `<sha>` <subject> (<date>) · kiwami: `<sha>` · base: `<sha>` (the last main merged) · Status: analysed | merged

## What upstream brings

The commits (`git log --oneline <base>..main`, how many), what they change by area (banana-ui, bananad, rootfs and
launchers, emulators and packages, kernel and board, build and CI, docs), and the versions that moved (Buildroot, its
Go, Linux, RetroArch, ...).

## Textual conflicts

What `git merge-tree` predicts, and what the merge then gave (rerere's resolutions included).

| File | Kind | The fork's side | Upstream's side | Resolution | Decided |
|---|---|---|---|---|---|
| `path` | content / modify-delete / rename / ... | | | | proposed / user / rerere |

## Logic conflicts

Upstream changes that merge cleanly but break, duplicate or change what the fork relies on. Each with what upstream
changed (commit, `path:line`), what of the fork's it affects, its severity (high / medium / low), and how it's
handled. Checklist areas with nothing found say so in a line.

1. ...

## Changes beyond resolving conflicts

What the merge commit changes in the fork's own files (or in hooks) to follow upstream, which the merge's diff
doesn't show plainly: each file and why. "None" if none.

## Unmerged branches

For each `k/*` branch not yet in kiwami: what upstream changed in files or behaviour it relies on, and what its merge
will meet. "None" if there are none.

## Checks

| Check | Result |
|---|---|
| No conflict markers left | |
| `make test` | |
| `make -C ui` | |
| `make -C system/bananad CFLAGS="-O2 -Werror"` | |
| `kiwami/tests/ci.sh .buildroot` | |
| The hooks: the same before and after (docs/FORK.md's two commands) | |
| What the fork shadows, read by hand | |
| Renders with the fork's options off match main's | |
| `kiwami/DEPENDENCIES.md`'s upstream table matches the new versions | |

## Decisions

Every question asked, the options offered and the user's answer.

## Left for later

What the merge doesn't settle and should get its own change (a fork feature upstream now also has, a hook upstream
could take, a test to add).
