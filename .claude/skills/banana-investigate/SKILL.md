---
name: banana-investigate
description: First stage of a BananaLinux fork (kiwami) feature. Takes the user's initial requirements, refines them with the user, writes the GitHub issue's title and description, analyzes the current codebase and upstream (main), points out what the requirements overlook, suggests improvements and alternative ways to the same result, and writes the investigation report to .workspace/<NNNN>-<name>/investigation.md. Use when the user wants to investigate, look into, scope or evaluate a new feature, change or bug for the fork, or starts a new feature.
user-invocable: true
disable-model-invocation: false
argument-hint: "[issue number, if the issue already exists] the feature, change or bug in your own words"
---

# Banana investigate

You are investigating this request for the fork of BananaLinux:

**$ARGUMENTS**

This is stage 1 of the fork's feature workflow (`CLAUDE.md`, *Working on a change*): **banana-investigate** →
`banana-architect` (the plan) → `banana-develop` (the steps, one commit each). The report you write is all the next
stages know about the request, so it has to be right, not just plausible.

Three things are never skipped, however simple the request looks:

1. **Ask instead of assuming.** Whenever you're unsure about scope, intent, expected behaviour or which way to do it,
   ask the user with `AskUserQuestion`. There is no quota: if the request and the code leave nothing open, don't
   invent questions. But the bar is low: if you're choosing between plausible readings or approaches, the choice is
   the user's. It is always better to ask than to guess.
2. **Read the requirements critically.** Point out what they overlook and what conflicts with existing behaviour or
   with upstream.
3. **Suggest.** Simpler ways to the same result, additions worth having, parts worth leaving out or postponing.

This stage only investigates: **never change source code, scripts, configuration or the build.** The only file you
write is the report. Read `CLAUDE.md` and `docs/FORK.md` first if they aren't already in your context; their rules
(where the fork's code goes, hooks, settings defaulting to upstream, dependencies, the user does all git) bind every
stage.

## Workspace

The feature's folder is `.workspace/<NNNN>-<name>/` at the repository root, committed with the feature:

- `<NNNN>` is the GitHub issue's number on this fork, four digits (issue #7 → `0007`).
- `<name>` is a short kebab-case name for the feature (`tailscale`, `7z-roms`), agreed with the user.
- The branch is `k/<NNNN>-<name>`, created by the user off `kiwami`.

The report is `.workspace/<NNNN>-<name>/investigation.md`, written from the template
[investigation.md](investigation.md) next to this file. Earlier features in `.workspace/` show the expected depth and
style (`.workspace/0003-7z-roms/investigation.md` is a good, short one), and may already cover parts of this request.

## Steps

### 1. Read the request

- If the arguments start with an issue number, the issue already exists: step 3 only checks its text.
- Decide whether this is a **feature** (new behaviour, a package, a screen, a service) or a **small change** (a fix,
  a setting, a doc). A small change doesn't need this workflow (`CLAUDE.md`): if it looks small, say so and why, and
  ask whether to go on with an investigation or do it directly on `kiwami`. Stop here if the user picks the latter.
- Pull out every concrete thing named: systems, emulators, settings, screens, files, packages, devices, behaviour.

### 2. First scan and question round 1: scope and intent

Look at the code just enough to understand today's behaviour and to ask informed questions (`docs/ARCHITECTURE.md`
for the area, `git grep` for the names in the request, the settings, the scripts in
`system/rootfs/usr/libexec/banana/`, the fork's `kiwami/` and `ui/src/kiwami/`, earlier `.workspace/` features).

Then ask with `AskUserQuestion` whatever the request and the code can't tell you, typically:

- what the feature is for and what "done" looks like, in the user's terms;
- what is in and out of scope (which systems, boards, panels, cards, settings);
- your understanding of the request stated back, so the user can confirm or correct it.

Wait for the answers. Skip the round only when nothing is open, and say why in the report.

### 3. The GitHub issue

Write the issue's text, in a code block ready to paste:

- a **title** in the repository's prose style (plain descriptive English, no conventional-commit prefix);
- a **description**: what and why, in a few short paragraphs, plus what is out of scope if that was decided.

Propose the feature's `<name>`. Then **stop and wait**: the user creates the issue by hand on this fork (not
upstream; `gh` isn't used) and the branch `k/<NNNN>-<name>` off `kiwami`, and replies with the number. If the issue
already existed, check its text against what step 2 settled and offer an updated text if it no longer matches.

When the user replies, run `git branch --show-current`. If it isn't `k/<NNNN>-<name>`, ask whether to go on anyway
(the report would then be committed on that branch) or wait until they've switched.

### 4. Deep analysis

Read the code involved, not just the names: trace the behaviour end to end (a button press through `main.cpp`'s loop
into the theme and settings, a launch through `systems.conf`'s `run` and the `run-*` launcher, a boot through `rcS`,
a suspend through `bananad` and `sleep.d/`, an image through the defconfigs, `post-build.sh` and `post-image.sh`).
Compare with upstream: `git diff main...kiwami -- <path>` and `git show main:<path>` show what is the fork's. Then go
through the checklist below, writing down every problem you find with its evidence (`path:line`).

### 5. Oversights and suggestions, question round 2

Write down the oversights (what the request doesn't cover or gets wrong) and the suggestions, each with what it would
mean, what it costs and your proposed answer. Then ask with `AskUserQuestion`:

- which of the options to take, with the trade-offs in the option descriptions;
- whether to accept each oversight's proposed answer and each suggestion that changes the scope or the approach
  (`multiSelect: true` for independent suggestions);
- anything the deep analysis left ambiguous.

If the answers open new questions, ask again. Stop when nothing blocking is left or the user doesn't want to answer
more. Suggestions you don't ask about (unrelated problems found nearby) stay listed as proposals, outside the scope.

### 6. Write the report

Write `investigation.md` from the template, every section filled (a section that doesn't apply says so in a line).
Record every decision where it belongs ("*Decided*: ..." under the oversight it settles) and every question in
*Open questions* with its answer. Score the confidence (below) and put it in the header, Status `draft`.

### 7. Present and refine

Give the user a summary: the request as understood, the oversights and suggestions with their decisions, the
recommended option, the risks, the confidence and the open questions, and the report's path. Refine the report with
the user until they're happy; then set Status to `refined`, `git add` the report, and give a commit message (below).
Stop. The next stage is `/banana-architect <NNNN>`, when the user says so.

## Checklist

Go through every row. For each that applies, write down what you checked and what you found; for each that doesn't,
"n/a: <why>" is enough. Don't skip a row because the change looks small.

| Area | What to check |
|---|---|
| **Upstream and the fork's rules** | Which upstream files would have to change, and whether a hook in the fork's own files (`docs/FORK.md`, *Where a customization goes*) can avoid it. Nothing in `ui/rnds/`, `ui/assets/rnds/`, `ui/assets/overrides/` or the ROCKNIX-synced files of `buildroot/board/rgds/`. How often upstream changes the files touched (`git log --oneline main -- <path>`). Behaviour changed from upstream's sits behind a setting defaulting to upstream's. |
| **Existing behaviour** | Every caller of what would change; other features that share the code; the fork's earlier features. Log lines `ui/tests/run.sh` asserts on. |
| **Both boards, both panels** | `rgds` and `rgds-plus` (and `universal`): panels, resolutions (640x480 and 1024x768), touch, which screen shows what. The device trees and defconfigs that differ. |
| **Sleep and the lid** | What happens when the handheld sleeps during it (`bananad`, `sleep.d/` `pre`/`post`, a game SIGSTOPped, Wi-Fi going down), and after resume (banana-ui woken by SIGUSR1). |
| **Boot, first boot, updates** | `rcS` order, `firstboot`, first-time setup; the A/B update (`update`, `post-image.sh`): what survives on `/data`, what a new image changes, a card set up by an older version. |
| **The card and the network** | No second card, a full or missing data partition, the user's own `/data/banana/` files (a `systems.conf` there replaces the image's), no Wi-Fi, Wi-Fi dropping. |
| **Games and emulators** | `docs/EMULATORS.md`'s contract, `systems.conf` (`run`, `ext`, `perf`), the `run-*` launchers, `game.pid`, display/input/sound handed to the game and back. |
| **Performance and footprint** | Menu frame time (under a millisecond), anything running while idle (polling, threads, daemons), rootfs size, memory, boot time, what a game loses. |
| **Dependencies** | New packages, libraries or toolchains, their versions and where they'd be pinned (`kiwami/DEPENDENCIES.md`); upstream's that the feature relies on (Buildroot, RetroArch's options, the kernel config). |
| **Errors and edge cases** | Empty, missing, duplicate and very large inputs; names with spaces or odd case; a failing command or download; what the user sees then. |
| **Security** | Anything listening on the network, credentials on the card (exFAT has no permissions), what an update or a bad file could do. |
| **Testing** | What `make test`/`ui/tests/run.sh` and `kiwami/tests/` can check headless (`--script`, fixtures, fake scripts like `ui/tests/fake-wifi.sh`), what `kiwami/tests/ci.sh` covers, what needs QEMU (`tools/qemu-boot.sh`), and what only the device can show and which log would show it. |

## Asking questions

- Use `AskUserQuestion`; never put a question in prose and carry on as if it were answered. (The issue number in
  step 3 is the exception: ask for it in prose and end your turn.)
- 1 to 4 questions per call, each with 2 to 4 concrete options. The recommended option goes first, with
  "(Recommended)" in its label; the descriptions say what each choice means for the feature.
- Ask only what the request, the code, the docs, git history or `CLAUDE.md` can't answer: what you can check, check.
- Ground each question in what you found (the file, setting or behaviour it's about).
- Never assume the answer to a question the user skipped: it goes in *Open questions* as *unanswered*, or *blocking*
  if planning can't go ahead without it.
- If you'd have to write "assuming", "probably" or "the user likely wants" about intent, scope or approach, ask.

## Confidence

Score the investigation before presenting it:

| Factor | Weight | Score |
|---|---|---|
| The request is clear | 20% | Clear from the request or confirmed with the user: 100%. Vague, unanswered: 20%. |
| The current state is understood | 30% | Traced in the code with `path:line`: 100%. Likely: 70%. A guess: 30%. |
| The files to change are known | 20% | All of them: 100%. Most: 70%. Unsure: 30%. |
| The way to do it is settled | 20% | Chosen by the user, or the only way that works: 100%. A general direction: 50%. None: 0%. |
| Nothing blocking | 10% | No open questions: 100%. Only *unanswered* ones: 60%. Any *blocking* one: 0%. |

The weighted sum is the confidence: 70% or more is *proceed* (recommend planning); 40 to 69% is *needs
clarification* (say which questions must be answered first); under 40% is *insufficient* (say what's missing, ask
for it, and don't recommend planning). A wrong investigation is worse than an incomplete one: an answer the user
couldn't give lowers the score, it isn't filled with a guess.

## Commit message

In the repository's style (`CLAUDE.md`, *Conventions*): a subject saying what changed, with the feature as its prefix,
and a body that says what the investigation found and decided, understandable from `git log` alone. No
`Co-Authored-By` or other AI attribution lines. For example:

```
7z ROMs: the investigation

.7z games are skipped because no system lists 7z in its ext line;
RetroArch is already built with 7z support, so adding it to the seven
cartridge systems in the image's systems.conf is enough. The DS and the
PlayStation are left out (no 7z in their launchers, and large ROMs
unpacked into RAM).
```

## Rules

- Ask with `AskUserQuestion` whenever unsure, before and after the deep analysis; never assume an answer to avoid
  asking.
- Never change anything but the report; never commit, switch branches or create the issue yourself.
- Every finding is backed by a `path:line`; facts (what the code does) are kept apart from guesses.
- Every checklist row is checked or marked n/a.
- Every oversight and suggestion has a decision, or is an open question.
- The scope is what the user agreed to: problems found nearby are suggestions, not part of the feature.
