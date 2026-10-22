---
name: banana-merge-upstream
description: Merges upstream BananaLinux (the main branch) into the fork's kiwami branch. Before the merge, predicts every textual conflict (a dry-run merge) and every logic conflict (upstream changes that merge cleanly but break, duplicate or change what the fork's hooks, build-time edits, packages and code rely on, including unmerged k/* branches), asks the user how to handle each, then has the user run the merge, resolves the conflicts, runs the checks and writes the report to .workspace/merges/. Use when the user wants to take in upstream, sync with upstream, merge main into kiwami, or update the fork.
user-invocable: true
disable-model-invocation: false
argument-hint: "(none: main must already be up to date with upstream)"
---

# Banana merge upstream

You are merging upstream's changes, on `main`, into the fork's `kiwami`, as `docs/FORK.md` (*Taking in upstream*)
describes:

**$ARGUMENTS**

The merge has three parts: **before** it, find everything that could go wrong and settle with the user how to handle
each; **the merge itself**, which the user runs; **after** it, resolve the conflicts as decided, adapt the fork where
upstream moved under it, check, and record. The point is that nothing upstream changed reaches `kiwami` unnoticed:
a clean textual merge isn't a correct one.

**Whenever you're unsure how to handle something, ask the user with `AskUserQuestion`; it is better to ask than to
assume.** Which side of a conflict wins, whether the fork's code is still needed, how to adapt a hook: those are the
user's choices.

Read `CLAUDE.md` and `docs/FORK.md` first if they aren't already in your context. **The user does all git** that
changes history, refs or branches: they bring `main` up to date, run the merge and commit it (or abort it). You use
read-only git, `git merge-tree` (it writes only loose objects, no refs, no working tree), edit the files, and `git
add`.

## The report

`.workspace/merges/<YYYY-MM-DD>-<main's short sha>.md`, from the template [report.md](report.md) next to this file.
It is committed in the merge commit itself, so the next merge can see how each conflict was settled. Earlier reports
in `.workspace/merges/` are worth reading first: a conflict settled once is usually settled the same way again.

## 1. Before anything

1. `git branch --show-current` is `kiwami`; `git status --short` shows no modified or staged tracked files; no merge
   is in progress (`git rev-parse -q --verify MERGE_HEAD` finds nothing). If one fails, say which and stop.
2. `git config --get rerere.enabled` and `git config --get merge.conflictStyle`: if they aren't `true` and `zdiff3`,
   show the user `docs/FORK.md`'s two lines (*Once, on each clone*) and ask (`AskUserQuestion`) whether they'll run
   them before the merge or go on without. Don't set them yourself.
3. `main` must already hold upstream's latest: the user brings it up to date (GitHub's *Sync fork* and a fast-forward
   of the local `main`, or `docs/FORK.md`'s commands). Don't fetch. Say which commit `main` is at and its date
   (`git log -1 --format='%h %ad %s' --date=short main`), and compare with `origin/main` as the local refs know it.
4. The range: `B=$(git merge-base kiwami main)`, the last `main` merged. If `B` is `main`, there's nothing to merge:
   say so and stop.

## 2. What upstream brings

- `git log --oneline --no-merges $B..main`, and `git diff --stat $B main`. Read the commits' messages and their diffs
  area by area; the report sums them up by area.
- The versions that moved: `git diff $B main -- Makefile .github/workflows/ci.yml buildroot/configs/
  buildroot/package/` for Buildroot, the kernel, RetroArch and the emulators, and `kiwami/DEPENDENCIES.md`'s second
  table for what the fork relies on.

## 3. Where the fork touches upstream

Build the list from the tree, not from memory; the fork grows with every feature:

- **Hooks**: `docs/FORK.md`'s two commands (*Hooks in upstream's files*). Save the `git grep` output: after the merge
  it must list the same hooks.
- **Upstream files the fork edits at build time**: every file `kiwami/post-build.sh` changes in `$T` (and the exact
  line shape it matches); every file in `kiwami/rootfs/` at the same path as one in `system/rootfs/` (the fork's
  replaces it); what `kiwami/configure.sh` assumes about the defconfigs' lists; `kiwami/configs/kiwami.fragment`'s
  options against the defconfigs; `kiwami/linux.fragment` against the kernel config.
- **What the fork's code calls**: the upstream headers, classes and functions `ui/src/kiwami/` uses (`git grep -n
  '#include "\.\./' ui/src/kiwami`, then what it calls from them); the scripts, paths (`/run/banana/`, `/data/banana/`),
  settings keys and log lines the fork's scripts and tests rely on (`kiwami/rootfs/`, `kiwami/tests/`); the CI
  workflow around the fork's step.
- **Files upstream owns that the fork edited outside a hook**: `CLAUDE.md` (its *This fork* and workflow sections),
  anything else `git diff --stat main...kiwami` shows that isn't the fork's own folder.
- **Unmerged branches**: `git branch --list 'k/*' --no-merged kiwami`, each with `git diff --name-only kiwami...<branch>`
  and its plan in `.workspace/`.

## 4. Textual conflicts: a dry run

```sh
git merge-tree --write-tree --name-only --messages kiwami main
```

Exit status 1 means conflicts: the first line is the merged tree's id, then the conflicted files, then git's messages
(content, modify/delete, rename/delete, add/add...). For each conflicted file, read the conflict in the merged tree
(`git show <tree>:<path>`, around the `<<<<<<<` markers), and both sides' history: `git diff $B main -- <path>` (what
upstream did) and `git diff $B kiwami -- <path>` (what the fork did), `git log --oneline $B..main -- <path>` (why).

Classify each and propose a resolution:

- **A hook's line moved**: upstream changed code around it. Put the hook's line back where upstream's code now is,
  unchanged.
- **Upstream rewrote what a hook calls into or hangs on**: the hook has to change to fit (a new function name, a new
  place in the loop). Propose the new hook; it's a decision for the user.
- **Both changed the same thing**: say what each side wanted; the options are upstream's, the fork's, or both
  combined.
- **Modify/delete, renames**: where the fork's hook or edit went, whether upstream moved the code elsewhere.
- **Fork-edited upstream docs** (`CLAUDE.md`): keep upstream's new text and the fork's sections.

Also read upstream's changes to every hooked file that merge **without** a conflict: a hook can merge cleanly into a
function that now does something else, or end up after an early return.

## 5. Logic conflicts

Go through every row against upstream's changes (`$B..main`). For each, write down what you checked and what you
found, with the commit and `path:line`, or "nothing found". Don't skip a row because the merge looks clean.

| Area | What to check |
|---|---|
| **Upstream does what the fork does** | Upstream adding the same feature or fix the fork carries (its own Tailscale, `7z` in `systems.conf`, a setting the fork added): two implementations side by side, or the fork's now redundant. Propose dropping or keeping the fork's. |
| **Hooks** | The code around each hook: does it still run when the hook meant it to (the same event, the same order, not after an early return)? Enums and lists a hook extends (`Settings::Page`, `Entry`): new values, reordering. |
| **What the fork's code calls** | Renamed, removed or changed classes, functions, struct fields, palettes, layouts in the headers `ui/src/kiwami/` uses: the build catches signatures, not a function that now behaves differently. |
| **Build-time edits** | The lines `kiwami/post-build.sh` matches changed shape (the build stops: good, but plan the fix), or the file's meaning changed (a new cartridge system that would want `7z`, a new place the card is shared that wants tailscaled's veto). Files the fork's overlay replaces that upstream changed: the fork's copy misses upstream's change. |
| **Build configuration** | Defconfig options the fragment overrides or relies on renamed, removed, or now set by upstream; Buildroot, Go, Linux or RetroArch versions moved (`kiwami/DEPENDENCIES.md`'s second table, and the fork's packages against the new Buildroot's infrastructure); kernel options in `linux.fragment` renamed or gone. |
| **Runtime contracts** | `rcS` and `rcK` order around the fork's lines (network, `/data` mounted, `sleep.d`), launchers, `systems.conf` fields, settings keys and files (`settings.conf`, `bananad.conf`), paths, `game.pid`, sleep and resume, SIGUSR1/SIGHUP, the A/B update and its package. |
| **The menu** | Settings pages and rows the fork's rows sit among (order, layout, the Network page), the screen's colours, touch areas, both panels and both sizes; what changes the renders (the baseline the final check compares with). |
| **Tests and CI** | `.github/workflows/ci.yml` around the fork's step; `ui/tests/run.sh`, `fixture.sh`, `fake-wifi.sh` and the log lines the fork's tests assert on; the Buildroot clone CI and `kiwami/tests/ci.sh` use. |
| **Docs** | `docs/ARCHITECTURE.md`, `docs/EMULATORS.md`, `CLAUDE.md`: what the fork's docs and `.workspace/` plans say that upstream's change makes wrong. |
| **Unmerged branches** | For each, upstream's changes to the files it touches and to the behaviour its plan relies on: what it will meet when it is merged. Only reported, the branch isn't touched. |

## 6. Settle it with the user

1. Write the report with what steps 2 to 5 found and the proposed handling of each, Status `analysed`.
2. Give the user a summary: what upstream brings, the textual conflicts with the proposed resolutions, the logic
   conflicts by severity, the adaptations the fork will need, and what the unmerged branches will meet.
3. Ask with `AskUserQuestion`:
   - every resolution that is a choice (which side wins, a new hook's shape, dropping the fork's code that upstream
     now duplicates, adapting a fork feature to upstream's new behaviour), with the trade-offs in the descriptions;
   - the mechanical ones (a hook's line put back where the code now is) together, for approval as a batch
     (`multiSelect: true` lets the user pick out the ones to discuss);
   - whether to go ahead with the merge now, or first do something on `main`'s side or in a separate change.
   Ask again if the answers open new questions. Record every question and answer in the report's *Decisions*.
4. Then ask the user to run the merge, and **stop and end your turn**:

   ```sh
   git merge --no-ff --no-commit main
   ```

   `--no-commit` holds the merge open even when nothing conflicts, so the resolutions, adaptations and the report all
   go into the merge commit. (To back out at any point: `git merge --abort`.)

## 7. Resolve

When the user says the merge is running:

1. Check it: `git rev-parse -q --verify MERGE_HEAD` is `main`'s commit; `git diff --name-only --diff-filter=U` lists
   the files still in conflict. Compare with the dry run. rerere may have resolved some already (`git rerere status`
   and `git rerere remaining`, `git diff` on them): check each of its resolutions against this merge's decisions, as a
   resolution recorded for an older upstream may no longer fit.
2. Resolve each file as decided, keeping both sides' code as agreed and no reformatting around hooks. `git add` each
   file once it's resolved.
3. Make the agreed adaptations to the fork's own code and hooks, and update `kiwami/DEPENDENCIES.md`'s upstream table
   when a version moved. These go into the merge commit; the report lists them under *Changes beyond resolving
   conflicts*.
4. A conflict or a problem the analysis didn't predict: stop and ask before resolving it.

## 8. Check

Run `docs/FORK.md`'s after-merge checks, with the merge still uncommitted:

- No conflict markers left anywhere: `git diff --cached --check` and a `git grep -nE '^(<<<<<<<|=======|>>>>>>>)( |$)'`
  of the files that were in conflict.
- `make test`, `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`, `kiwami/tests/ci.sh .buildroot`. If
  upstream moved Buildroot, `.buildroot` has to be the new version first (the top `Makefile` clones it; ask the user
  before replacing the existing clone).
- The hooks: the `git grep` from step 3 against the one now. The same hooks, each on the line it should be; a hook
  upstream's change removed is a hook gone, not a conflict.
- What the fork shadows: upstream's changes to the files from step 3 (*Upstream files the fork edits at build time*)
  read by hand, against what the fork does to them.
- The renders: with the fork's options off, `make test`'s renders in `ui/tests/out/` against `main`'s, rendered from
  a second checkout (ask the user to create it with `git worktree add ../banana-main main`, or for permission to).
  Compare in the same minute: the clock is on some screens.

A failing check is fixed in the merge commit when the fix is part of following upstream (the user decided that
adaptations go there); anything larger, ask: fix it now, or abort the merge and do it as its own change first. Never
weaken a test to get green.

## 9. Record and hand over

1. Finish the report: what the merge gave, each resolution and adaptation as done, the checks' results, *Left for
   later*; Status `merged`. `git add` it, and check `git diff --cached --stat` holds only the merge's files.
2. Give the user a summary (conflicts resolved, adaptations, checks, what the unmerged branches will meet) and the
   merge commit's message in a code block: the subject `Merge branch 'main' into kiwami`, and a body in the
   repository's style (plain descriptive English, understood from `git log` alone, no `Co-Authored-By` or other AI
   attribution lines): what upstream brought in a few lines, how the conflicts were settled, what the fork changed to
   follow upstream, and the report's path. Upstream's changes are named by what they do (or a commit's short SHA),
   never by their pull request or issue: an upstream subject's "(#12)" copied here would link this fork's issue 12 or
   upstream's item 12, and an `<owner>/<repo>#12` or a link would show the fork's merge on upstream's page
   (`CLAUDE.md`, *This fork*). For example:

   ```
   Merge branch 'main' into kiwami

   Upstream brings PlayStation launching through run-ps1, idle dimming
   in bananad and RetroArch 1.22.2. Settings' Network page conflicted
   at the fork's Tailscale row: the hook is back below Wi-Fi, where
   upstream's rows now start. kiwami/DEPENDENCIES.md has the new
   RetroArch. Report: .workspace/merges/2026-10-21-8960629.md
   ```

3. Ask the user to commit it (`git commit`, with that message) and stop. Don't push: that, too, is the user's.

## Asking questions

- Use `AskUserQuestion`; never put a question in prose and carry on as if it were answered. (Asking the user to run
  the merge is the exception: say it and end your turn.)
- 1 to 4 questions per call, each with 2 to 4 concrete options; the recommended one first, with "(Recommended)" in
  its label; the descriptions say what each choice means for the fork.
- Ground each question in what you found: the commit, the file and line, both sides' code.
- Ask only what the code, the history, the earlier reports and the docs can't answer.
- Never resolve a conflict past a question the user didn't answer: stop and say what it blocks.

## Rules

- Analyse before the merge, every checklist row, and settle every choice with the user first.
- The user brings `main` up to date, runs the merge, commits or aborts it, and pushes; you never fetch, merge, commit,
  reset, stash, checkout or push. `git merge-tree`, read-only git and `git add` are yours.
- rerere's resolutions are checked, not trusted.
- Hooks stay one marked line (or a few), with no reformatting of upstream's code around them; after the merge, the
  same hooks are there.
- Every resolution and adaptation is in the report, committed with the merge.
- Report failures as they are, with the command and its output; let the user decide.
