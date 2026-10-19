# <NNNN> <Feature>: investigation

Issue: #<N> · Branch: `k/<NNNN>-<name>` · Status: draft | refined

## The request

What the feature is for and what the user wants from it, in their terms. What is out of scope.

## Oversights in the requirements

What the request doesn't cover or gets wrong: cases left out, conflicts with existing behaviour or with upstream, what
happens around sleep, on both boards and both panels, on first boot, across updates, without a card or a network.
Each one with what it would mean and a proposed answer, for the user to decide.

## Suggestions

What would make the feature better: a simpler way to the same result, additions worth having, parts worth leaving out
or postponing. Each one with what it costs.

## Current state

What the project does today in this area, with the files and functions involved (`path:line`), and how upstream
(`main`) does it. What already exists that the feature can build on (settings, hooks, scripts, packages).

## What has to change

The pieces of work the feature needs, without the step-by-step detail (that's the plan): new files in the fork's
places, hooks in upstream's files (each one named, with why it can't be avoided), settings and their upstream defaults,
packages, rootfs additions.

## Options

The ways it could be done, each with what it costs and what it gives, and the one recommended, with why.

## Risks

- Performance and footprint: frame time, idle activity, rootfs size, memory, boot time.
- Conflicts with upstream: which upstream files are touched, how often upstream changes them.
- Hardware and boards: `rgds`, `rgds-plus`, what only the device can show.
- Anything else that could go wrong.

## Testing

What can be tested headless (`kiwami/tests/`, `make test`), what only on the device, and which logs would show it.

## Open questions

Questions for the user, each answered here once settled (and the answer moved into the sections above).

1. ...
