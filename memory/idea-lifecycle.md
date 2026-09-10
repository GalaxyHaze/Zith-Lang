# Idea Lifecycle Memory

This note records where idea lifecycle artifacts live and why the repo
requires file-backed drawing, signing, and conforming. It is project memory,
not a tutorial.

The repository uses `docs/specs/abi-lifecycle.md` as the source of truth for
the ABI lifecycle: drawing, promising, signing, conforming. `docs/agents/idea-flow.md`
is the operational route from an idea to signed ABI work.
`docs/plans/idea-lifecycle-small-scope.md` is the small-scope rollout plan.

## Why File-Backed Drawing

An idea must live on disk before technical work starts. A chat-only idea has no
commit, no review surface, and no invalidation point. The file is the primary
source; the conversation is context.

This is a deliberate choice, not a documentation preference. It makes the
compliance check possible: the latest plan, promises, and tests are all in the
repo and can be compared.

## Artifact Map

| State | Home | Required content |
|---|---|---|
| Drawing | `docs/plans/<area>/<idea>.md` | status, problem, size, non-goals, open decisions |
| Promising | same plan, `## Promises` | stable ids, seam, layout, reverse |
| Signing | `docs/adr/` | promise ids, plan link |
| Conforming | `tests/` | one test per promise id |

## Struct Layout As The Smallest Unit

Layout-sensitive work is dimensioned by struct layout, not by feature count.
Before a layout rule is promised, the field, byte offset, size, alignment, and
target ABI must be named. Otherwise the promise cannot be tested.

## Small-Scope Contracts

Small teams keep the same lifecycle. The cost control is the size rule, not
skipping states. One idea fits one plan file; larger work splits the drawing,
not the discipline.

## Maintenance

Update this note when the artifact map changes. Update
`docs/specs/abi-lifecycle.md` when the contract model changes. Update
`memory/plan-debt-status.md` when an active plan is archived.
