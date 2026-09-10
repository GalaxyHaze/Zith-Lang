# ABI Lifecycle

Status: draft

## Purpose

The ABI lifecycle defines how every externally visible contract in Zith moves
through four states: drawing, promising, signing, and conforming. It exists so
an idea is not turned into code by technical rehearsal before its promises are
explicit, and so a changing idea invalidates stale promises instead of
silently diverging from tests.

## Contract

The project uses the four-state model for any contract that crosses a compiler
or runtime boundary. A contract is a named interface with declared inputs,
outputs, invariants, failure modes, and layout obligations.

1. **Drawing**: the idea exists as a concise markdown brief with the problem,
   the non-goals, and the open decisions. Nothing here is promised.
2. **Promising**: the technical facts are settled enough to compute concrete
   promises. A promise states an observable rule, not an implementation
   preference. It names the test seam, the layout obligations, and the cases
   that are outside the contract.
3. **Signing**: an ADR accepts the promises as the contract that code and tests
   must satisfy. This is the point of no cheap reversal.
4. **Conforming**: tests prove the promises against the chosen seam. Each test
   carries the promise id it verifies. Code that cannot be observed through a
   contract test is not ABI-conforming work.

## Entry, Change, And Exit

An idea enters the lifecycle as a drawing. The drawing must be small enough to
read in one sitting and must name its size: single unit, module, or system.
The size is the primary scoping decision. A drawing that cannot name its size
is not ready for technical work.

A drawing becomes promising when the technical owner has checked the facts and
converted each open decision into a promise or a rejected option. Promises are
reviewed before signing, not after implementation. The reviewer checks that
each promise is observable through a named seam, that the layout obligations
match the intended struct layout, and that no promise depends on an unsigned
decision.

Signing publishes an ADR in `docs/adr/`. The ADR lists the promise ids it
accepts and links to the plan that contains the technical detail. Until the
ADR exists, the plan is a candidate, not a contract.

Conforming writes tests before implementation is treated as done. The code may
be written after the tests are drafted, but the first red state must be a
contract test, not an internal unit test. A conforming change updates or
archives the drawing and plan when the contract changes.

## Struct Layout As The Unit Of Structure

Struct layout is the smallest object the lifecycle treats as a first-class
unit. An ABI line is one concrete field/offset/size/alignment row in a layout
contract. Every rule that mentions an ABI line must define that row before the
rule can be promised.

The lifecycle uses the following rule: a contract with a struct layout
promises a stable layout only when the drawing names the target ABI, the
promise records `@sizeOf`, `@alignOf`, field offsets, and field types, the
ADR cites those values, and the conforming tests compare the implementation
against those values.

This does not mean every struct needs an ADR. It means layout-sensitive rules
are dimensioned by layout, not by code size or feature count.

## Seams

A seam is the place where a contract can be observed and tested. The lifecycle
prefers one seam per contract. A seam is named in the drawing and recorded as
the promise's test target.

The recommended seam for ABI work is a standalone executable under `tests/`
that links against the public surface of the artifact. Internal implementation
is not a seam unless the contract is genuinely internal and no public seam can
observe it.

## Promise Format

Each promise uses a stable id and the following fields:

```text
id: ABI-<area>-<NN>
contract: <named contract>
rule: <one observable rule>
seam: <test target>
layout: <struct layout obligation or "none">
fields: <one row per field/offset/size/alignment or "none">
reverse: <the decision that would invalidate this promise>
```

The `reverse` field is mandatory. A promise without a named reversal is not a
promise.

## Compliance Check

The lifecycle requires a compliance check at every transition. The check asks
three questions:

1. Does the latest drawing still match the signed contract?
2. Do the promises still match the latest drawing?
3. Do the tests still match the signed promises?

If any answer is no, the transition is blocked. The blocking document is the
one that changed first: a changed idea updates the drawing, a new technical
fact updates the promises, and a failing test updates the implementation.

The compliance check is run before a plan is archived, before a promise id is
reused, and before code lands that changes a signed contract. It is not run
before every commit.

## Lifecycle States In The Repository

| State | Artifact | Gate |
|---|---|---|
| Drawing | `docs/plans/<idea>.md` | one-page brief with size and non-goals |
| Promising | same plan plus `## Promises` | every promise has seam, layout, reverse |
| Signing | `docs/adr/NNNN-<idea>.md` | ADR cites promise ids |
| Conforming | `tests/test-<abi>.cpp` | each promise id has a test, suite green |

The plan doubles as the technical shelf for the idea. The drawing and the
promises live in the same file so a reader sees the idea and its promise
surface together.

## Small-Scope Organisation

When a team or repository is small, the lifecycle is still a file system, not
a ceremony. The rules that matter are these:

- One small directory holds the active ideas.
- Every idea is one file until it earns a split.
- Every plan names its size in the first five lines.
- Promises stay inside the plan until signed.
- Tests are written against signed promise ids.
- A changed idea archives or rewrites its plan, never patches code first.

The small-scope rollout keeps the same transitions but treats a single feature
as the smallest complete lifecycle. A team of one still performs drawing,
promising, signing, and conforming. The artifacts are just smaller.
