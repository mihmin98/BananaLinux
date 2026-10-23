---
name: banana-architect
description: Second stage of a BananaLinux fork (kiwami) feature. Reads the investigation in .workspace/<NNNN>-<name>/investigation.md written by banana-investigate, explores the code, and writes the implementation plan to .workspace/<NNNN>-<name>/plan.md - acceptance criteria, the work split into steps that are one commit each, the automated tests, the manual tests on the device with the logs to collect, and the progress table. Use when the user wants to plan, design or break down a feature after its investigation is done.
user-invocable: true
disable-model-invocation: false
argument-hint: "<NNNN> or <NNNN>-<name> (the feature's folder in .workspace/)"
---

# Banana architect

You are planning the feature given here:

**$ARGUMENTS**

This is stage 2 of the fork's feature workflow (`CLAUDE.md`, *Working on a change*): `banana-investigate` →
**banana-architect** → `banana-develop`. The plan you write is what `banana-develop` implements, one step and one
commit at a time, and what the user reviews before any code is written. Each step has to be exact enough to implement
without re-investigating, and small enough to review as one commit.

**Whenever you're unsure of something, ask the user with `AskUserQuestion`; it is better to ask than to assume.** A
plan built on a guess sends every step the wrong way; a question costs one round-trip.

This stage only plans: **never change source code, scripts, configuration or the build.** The only file you write is
the plan. Read `CLAUDE.md` and `docs/FORK.md` first if they aren't already in your context; their rules bind every
step you plan.

## Workspace

- Read: `.workspace/<NNNN>-<name>/investigation.md`. The argument is the number (`7`, `0007`) or the folder's name
  (`0007-tailscale`); find the folder with `ls -d .workspace/<NNNN>-*`. If there is none, or more than one, say so
  and stop.
- Write: `.workspace/<NNNN>-<name>/plan.md`, from the template [plan.md](plan.md) next to this file. Earlier
  features' plans in `.workspace/` show the expected detail (`.workspace/0003-7z-roms/plan.md`).

## Steps

### 1. Read the investigation

1. Read the investigation whole: the request, the decisions on each oversight and suggestion, the option chosen, the
   risks and the open questions.
2. If its Status isn't `refined`, or its confidence is under 70%, or an open question is *blocking*: stop and say the
   investigation needs more work (`/banana-investigate`) before planning.
3. Ask the user, with `AskUserQuestion`, each open question marked *unanswered*. If the user answers, plan with the
   answer and add it to the investigation's *Open questions* (the report is part of the record; it gets staged with
   the plan). If one stays unanswered and the plan depends on it, stop and say which one. Don't take the recommended
   option for the user, and don't plan two ways round a gap.
4. Run `git branch --show-current`; if it isn't `k/<NNNN>-<name>`, ask whether to go on anyway.

### 2. Explore the code

Build on what the investigation found; verify it rather than trusting it (the code may have moved since):

- Read every file the plan will touch, and find the exact place each change goes (`path:line`, the function).
- For each hook in an upstream file: the line it goes on, and that `kiwami:` marks it. Check that no change lands in
  `ui/rnds/`, `ui/assets/rnds/`, `ui/assets/overrides/` or the ROCKNIX-synced files of `buildroot/board/rgds/`.
- Find an existing pattern to follow for the code (the fork's earlier features in `kiwami/`, `ui/src/kiwami/`; upstream's
  own code nearby) and **for the tests** (`kiwami/tests/*.sh` for headless banana-ui runs and stand-in scripts,
  `kiwami/tests/units.cpp` for unit tests, `kiwami/tests/ci.sh` for what CI runs, `ui/tests/run.sh` for upstream's).
- How each step will be checked: which make target, which test script, which `--script` run.

### 3. Acceptance criteria

Write what has to be true when the feature is done, numbered `A1`, `A2`, ... so the tests can name them: given the
situation, when the user (or the system) does something, then what happens. Include the edge cases, the error cases
and the oversights the investigation decided (both boards, sleep, first boot, an update, no card, no network) where
they apply. Each criterion has to be checked by at least one automated test, or by a manual test when only the device
can show it. If one can't be checked at all, reword or split it, and ask the user if that changes what it means.

### 4. Split the work into steps

- **One step is one commit**, reviewable on its own: a cohesive piece (a package and its `DEPENDENCIES.md` line; a
  helper script and its stand-in test; a settings row and its headless check; a hook and the code it calls).
- **Every step leaves the tree building and the tests passing** (`make test`, `kiwami/tests/ci.sh .buildroot`, and
  `-Werror` builds where the step touches C/C++). Never a step that breaks something a later step repairs: merge
  them.
- **The fork's rules hold within each step**: a hook goes in the same commit as the code it calls; a dependency goes
  in `kiwami/DEPENDENCIES.md` in the commit that adds or bumps it; a doc that the step makes wrong (`docs/FORK.md`,
  `docs/ARCHITECTURE.md`, `CLAUDE.md`) is updated in that step.
- **Order by dependency**: what a step needs comes in an earlier step, never a later one.
- **Each step marks its own Progress row done**, in its own commit (the template's Progress section).
- **A bug fix starts with a test that reproduces it**: the step says which test fails before the fix, and how (the
  message or the wrong output), then the fix makes it pass, all in one commit.
- Smaller is better, but not below a meaningful commit. If how to split is not obvious, ask.
- **The last step is always the final check** (the template's list, adapted to what this feature touches).

Each step, in the template's form:

- **Files**: every file, each marked `new`, `hook` (an upstream file, the `kiwami:` line) or `changed`.
- **Change**: exactly what is added or changed and why: the function and its behaviour, the setting with its name,
  values and upstream default, the hook's line and where it goes, the error cases. A sketch of the code where the
  shape matters (an awk program, a function's signature, a settings row).
- **Tests**: the tests added or extended (script and what it asserts, with concrete values), and the commands that
  must pass at the end of the step.
- **Commit**: the subject, in the repository's style, with the feature as its prefix (`7z ROMs: ...`).

### 5. Tests

- **Automated**: one row per test, in `kiwami/tests/` (hooked into `kiwami/tests/ci.sh` so CI runs it), with what it
  checks and the criteria it covers. Prefer headless checks: banana-ui `--headless --script` runs asserting on its
  log lines and renders, stand-in scripts for commands the host doesn't have (`kiwami/tests/bin/`), unit tests in
  `units.cpp`, `post-build.sh` run against a stand-in target.
- **Manual**: what only the device (or QEMU) can show: sleep and the lid, hardware, Wi-Fi, a real update, timings.
  Each row with the board, the exact steps, what's expected, the logs to collect and the criteria it covers. Where
  the feature has no log of its own to show it, plan for it to log to `/data/banana/logs/`; say how to fetch each log
  (over SSH/SFTP, Settings > Network > SSH, or from the card on a computer). Ask the user which boards they can test
  on if you don't know.
- With the fork's options off, the fork must look as `main` does: the final check compares `make test`'s renders.

### 6. Risks

In the plan's *Risks*, say what could break and what guards it: the tests that cover it, or why a risk not covered by
any test is acceptable. Performance and footprint (frame time, idle activity,
rootfs size, memory, boot time) and conflicts with upstream always get a line.

### 7. Check the plan

Before presenting it, fix the plan until each holds:

- [ ] Every acceptance criterion is covered by at least one automated or manual test.
- [ ] Every step has Files, Change, Tests and Commit filled, and a row in Progress; the last one is the final check.
- [ ] No step leaves the tree failing; no step needs a later one.
- [ ] Every `changed` or `hook` file exists, and every `new` file's folder exists or is created by the step (check
      with `ls`/`git ls-files`).
- [ ] Every upstream file touched is a `hook` with a `kiwami:` line, and none is in the vendored or synced folders.
- [ ] Every new behaviour that changes upstream's sits behind a setting defaulting to upstream's.
- [ ] Every dependency added or bumped is in `kiwami/DEPENDENCIES.md` in the same step.
- [ ] Every bug fix's step has its failing test first.
- [ ] Every manual test says which logs to collect and how.

### 8. Present and agree

Give the user, in the chat: the summary, the acceptance criteria, the steps (number, title, files, commit subject),
the tests, the risks and the questions you asked with their answers, and the plan's path. Discuss and change the plan
until the user approves it; then set Status to `approved`, `git add` the plan (and the investigation if step 1 changed
it), and give a commit message (below). Stop. The next stage is `/banana-develop <NNNN>`, when the user says so.

## Asking questions

Ask at any point, as often as needed. Typical reasons:

- the investigation leaves a choice open, or two existing patterns fit and the choice changes the design;
- a setting's name, values, default or place in Settings isn't settled;
- the scope of a criterion or an edge case depends on what the user wants;
- how to split the work into commits isn't obvious;
- a risk or a trade-off (footprint, a hook in a busy upstream file) the user should weigh.

How:

- Use `AskUserQuestion`; never put a question in prose and carry on as if it were answered, and never fill a gap with
  an assumption.
- 1 to 4 questions per call, each with 2 to 4 concrete options; the recommended one first, with "(Recommended)" in
  its label; the descriptions say what each choice means for the plan.
- Ask only what the investigation, the code, the docs or git history can't answer.
- Wait for the answers before planning what depends on them. A question the user skips goes in the investigation's
  *Open questions* as *unanswered*, and the plan doesn't guess around it.

## Commit message

In the repository's style (`CLAUDE.md`, *Conventions*), no `Co-Authored-By` or other AI attribution lines: the
subject `<Feature>: the plan`, the body the approach and the steps in a few lines. For example:

```
7z ROMs: the plan

Three steps: post-build adds 7z to the seven cartridge systems' ext
lines in the image's systems.conf (and stops the build if one is gone);
a headless check that .7z games are listed on their shelves; the final
check. Manual tests on the RG DS Plus cover both cards, every emulator,
saves, sleep and the N64's start time and memory.
```

## Rules

- Read the investigation first; it is the record of what the user decided.
- Ask with `AskUserQuestion` whenever unsure; never assume an answer, a scope or a design choice.
- Explore the code before designing; follow the patterns already there.
- One step, one commit, the tree passing after each; the final check last.
- Every step concrete: files, functions, settings and tests named, not described.
- Never change anything but the plan (and the investigation's answered questions); never commit or switch branches.
