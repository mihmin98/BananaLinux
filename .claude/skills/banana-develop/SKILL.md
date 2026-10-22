---
name: banana-develop
description: Third stage of a BananaLinux fork (kiwami) feature. Implements the approved plan in .workspace/<NNNN>-<name>/plan.md written by banana-architect, one step at a time - implements the step, runs its tests, marks it done in the plan's progress table, stages the files and gives the user a commit message, then stops until the user has committed. Also runs the manual tests' log analysis, the final check, and gives the pull request's text. Use when the user wants to implement, develop or code a planned feature, or says "next step", "continue the plan", "go on".
user-invocable: true
disable-model-invocation: false
argument-hint: "<NNNN> or <NNNN>-<name> [step number] (the step defaults to the first one not done)"
---

# Banana develop

You are implementing the plan of the feature given here:

**$ARGUMENTS**

This is stage 3 of the fork's feature workflow (`CLAUDE.md`, *Working on a change*): `banana-investigate` →
`banana-architect` → **banana-develop**. You implement **one step at a time**, in this conversation: implement it,
check it, stage it, give a commit message, and **stop**. The user commits it by hand and tells you to go on.

**Whenever you're unsure of something, ask the user with `AskUserQuestion`; it is better to ask than to assume.** That
holds for anything the plan doesn't settle: a name, a value, a place in the code, a test that can't be written as
planned.

Read `CLAUDE.md` and `docs/FORK.md` first if they aren't already in your context. Their rules bind every step: the
fork's code in its own files, upstream's files changed only for a `kiwami:` hook with no reformatting around it,
changed behaviour behind a setting defaulting to upstream's, dependencies in `kiwami/DEPENDENCIES.md` in the same
commit, and **the user does all git**: you only `git add`.

## Workspace

`.workspace/<NNNN>-<name>/`: the argument is the number (`7`, `0007`) or the folder's name; find it with
`ls -d .workspace/<NNNN>-*`. It holds:

- `investigation.md`: the request and its decisions; read it for the why behind the plan.
- `plan.md`: the source of truth for what to do. Its *Progress* table is the record of where the work is, kept in git
  by each step's own commit; *Deviations* records where the work turned out different; *Manual tests* collects the
  outcomes.

## 1. Find the step

1. Read `plan.md` and `investigation.md`. If the plan is missing or its Status isn't `approved`, say so and stop
   (`/banana-architect` comes first).
2. Run `git branch --show-current`; if it isn't `k/<NNNN>-<name>`, ask whether to go on anyway.
3. The step is the one given in the arguments, or else the first row of *Progress* that isn't `done`.
4. **Starting the first step needs the user's permission** (`CLAUDE.md`): the user invoking `/banana-develop` or
   saying "start" is that permission; otherwise ask.

## 2. Check the previous step was committed

Before any step after the first:

1. `git status --short`: files of the previous step still modified or staged mean it hasn't been committed yet.
2. `git log -1 --format=%s`: the subject should be the previous step's commit in *Progress*.

If either doesn't match, ask the user (`AskUserQuestion`) whether it's been committed under another message, should
wait, or whether to go on anyway. Untracked files unrelated to the plan are fine; leave them alone.

## 3. Implement the step

1. Read the step's Files, Change and Tests, and every file it touches, before changing anything.
2. Make the changes as planned, in the code's own style: its comment density, naming and idiom. In an upstream file,
   only the hook's lines, each marked `kiwami:`.
3. A bug fix: write the test that reproduces it first, run it and see it fail the way the plan says, and only then
   fix. If it passes before the fix, stop and say so: it doesn't reproduce the bug.
4. Write the step's tests as planned, hooked into `kiwami/tests/ci.sh` where the plan says.
5. Run the step's checks and whatever the touched code needs, until all pass:
   - `make test` (banana-ui and its renders), `kiwami/tests/ci.sh .buildroot` (the fork's scripts, tests and every
     board's configuration);
   - `make -C ui` when banana-ui's code changed, `make -C system/bananad CFLAGS="-O2 -Werror"` when bananad's did;
   - `sh -n` on every shell script added or changed;
   - anything else the step names (a `--headless --script` run, a Buildroot target for a package).
   A failing test is fixed in the code, never by weakening, skipping or deleting the test or loosening its expected
   value. If the plan's expected value is provably wrong, that's a deviation (below).
6. Set the step's *Progress* row to `done`, with its commit subject. If the step changed anything in the plan's other
   sections (a test's outcome, a deviation), that goes in too: it's the same commit.

**If the step turns out different from the plan** (a file elsewhere, another approach, a test that can't be written
as planned, a step that has to split or merge): stop before going further, explain why, and ask the user. Once they
agree, add a dated entry to the plan's *Deviations* (what changed and why) and update the steps it affects, then go
on.

## 4. Stage, report, and stop

1. `git add` exactly the step's files and `plan.md` (by name, never `git add -A` or `.`). Then `git diff --cached
   --stat` and compare it with the step's Files: anything missing, or staged but not planned, is explained or fixed.
   For a hook, `git diff --cached` shows only the hook's lines in the upstream file.
2. Tell the user, briefly:
   - **Step <n>/<total>: <title>**, done;
   - the files changed, and what each change does;
   - the tests and checks run, with their results;
   - deviations from the plan, or "none";
   - the **commit message** in a code block, ready to use (below);
   - the next step's title.
3. **Stop and end your turn.** Don't commit, and don't start the next step until the user says to go on.

If the user asks for changes to the step instead, make them, run its checks again, re-stage, and give an updated
report and commit message.

## 5. Manual tests

The plan's manual tests usually need an image built from the branch (`make rgds`, `make rgds-plus`), which the user
builds and flashes, or that you build when the user asks (it takes over an hour; run it in the background). When the
plan reaches them:

- Give the user the checklist: each test's board, steps, what's expected, and the logs to fetch, how (over SSH/SFTP,
  Settings > System > SSH, or from the card on a computer; `/data/banana/logs/` is `banana/logs/` on the card) and
  under which name.
- When the user passes logs or outcomes back, analyse them against the expected behaviour, and say what they show
  with the lines that show it. A failure is diagnosed from the logs; a fix is a change to the plan agreed with the
  user (a *Deviation*, a new step or a fix to a step not yet committed), not a silent patch.
- Record each outcome in the manual tests table (`passed`, `failed`, `skipped` with why) and what the logs showed in
  its notes, dated, with the image's version and the commit it was built from. That change is staged and committed
  like a step (`<Feature>: M3's outcome on the RG DS Plus`), unless it goes into the final check's commit.

## 6. The final check

The plan's last step. Run everything it lists and record the results; it changes no code:

- `make test`, `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`, `kiwami/tests/ci.sh .buildroot`.
- The hooks: the two commands in `docs/FORK.md` (*Hooks in upstream's files*, with `main...HEAD` for this branch)
  agree with each other, and list only the hooks this feature meant to add.
- The renders: with the fork's options off, `make test`'s renders in `ui/tests/out/` match those of the baseline
  (`main`'s, rendered from a second checkout: ask the user to create the worktree, or for permission to create it;
  if the fork already differs from `main` on screen for an earlier feature, `kiwami` before this feature is the
  baseline, recorded as a deviation). Renders taken in different minutes differ in the clock: compare in the same
  minute.
- `kiwami/DEPENDENCIES.md`, `docs/FORK.md`, `docs/ARCHITECTURE.md`, `CLAUDE.md` up to date with the feature.
- Every acceptance criterion met: each listed by at least one automated or manual test, and those passed.

If anything fails, say so plainly first, and stop: the user decides whether to fix it in a new step, change the plan
or accept it. When it all holds, mark the final check done in *Progress*, stage `plan.md`, and give its commit message
(`<Feature>: the final check done`, the body saying what passed and what was compared with what).

## 7. The pull request

After the final check is committed, give the user the pull request's text: a **title** in the repository's style (it
becomes the merge commit's subject, e.g. `7z ROMs: .7z games listed and played on the seven cartridge systems`) and a
**description** (what the feature does, how, what was tested on which board, anything left for later), ending with
`Closes #<issue>`. Give the **link** that opens the pull request with this fork's `kiwami` as its base and the
feature's branch as its head:

```
https://github.com/<owner>/<repo>/compare/kiwami...k/<NNNN>-<name>
```

with the owner and repository taken from `git remote get-url origin` (`git@github.com:<owner>/<repo>.git` or
`https://github.com/<owner>/<repo>.git`). Remind the user:

- the base must be this fork's `kiwami`, not upstream's repository (GitHub defaults to the parent; the link sets it);
- merge with "Create a merge commit" (not squash or rebase), so the step commits that *Progress* names stay in the
  history.

## Commit messages

In the repository's style (`CLAUDE.md`, *Conventions*), written to be understood from `git log` alone: the subject is
the step's commit subject from the plan (adjusted only if the step deviated), the body says what the change does and
why, and how where the diff doesn't make it obvious. Plain descriptive English, no conventional-commit prefixes, and no
`Co-Authored-By` or other AI attribution lines. Neither these nor the pull request's text refer to upstream's or
another repository's issues and pull requests, nor `@` mention their people (`CLAUDE.md`, *This fork*): a bare `#<n>`
is written only for this fork's own issues, never with an upstream number. For example:

```
7z ROMs: the cartridge systems list .7z games

post-build.sh adds 7z to the ext lines of gba, gb, gbc, nes, snes,
megadrive and n64 in the image's copy of systems.conf, so their .7z
games are listed; RetroArch already unpacks them. A system with no ext
line stops the build, so an upstream change to the list is noticed, and
a line that already has 7z is left alone.
```

## Asking questions

- Use `AskUserQuestion`; never put a question in prose and carry on as if it were answered.
- 1 to 4 questions per call, each with 2 to 4 concrete options; the recommended one first, with "(Recommended)" in
  its label; the descriptions say what each choice means.
- Ask only what the plan, the investigation, the code or the docs can't answer; what you can check, check.
- Never guess past a question the user didn't answer: stop and say what it blocks.

## Rules

- The plan is the source of truth; anything different is a deviation, agreed with the user and recorded.
- One step at a time; stop after each, and never start the next one unprompted.
- Every step leaves the tree building and its tests passing; never weaken a test to get there.
- Each step marks its own *Progress* row in its own commit.
- `git add` only the step's files; never commit, amend, stash, reset, switch branches or push.
- Report failures as they are, with the command and its output; don't loop on them, let the user decide.
