# Idea Lifecycle, Small-Scope Setup

This plan sets up a small-scale version of the idea lifecycle that already
exists for Zith infrastructure. It targets one person or a small team where
the cost of documentation must stay below the cost of lost design.

Status: active

Scope: process documentation and repository layout only. No compiler source
changes in this plan.

## Decision

Use one small directory per active idea, with every idea as one file until it
earns a split. The canonical lifecycle is `draw -> promise -> sign -> conform`,
defined in `docs/specs/abi-lifecycle.md`.

The lifecycle is not optional at feature size. Small is a property of the idea,
not of the discipline.

## File Layout

```text
docs/plans/<area>/<idea>.md   drawing and promises for one active idea
docs/adr/                     signed decisions
docs/specs/                   specs with mandatory contracts
memory/<area>-<topic>.md      durable project memory, one topic per file
.scratch/<feature>/issues/    local issues when a real tracker is absent
```

The plan index lives in `memory/plan-debt-status.md`. The ABI lifecycle spec is
the source of truth for state transitions.

## Workflow

1. Write the idea as a drawing with a size, a problem, non-goals, and open
   decisions.
2. Commit the drawing before asking for technical opinions. A drawing that is
   not on disk does not exist.
3. Compute promises only after the drawing accepts review. Each promise is a
   rule with a seam, layout obligation, and named reversal.
4. Sign accepted promises in an ADR. The ADR points to the plan and promise
   ids.
5. Write ABI tests first for the signed promises, then implement to green.

## First Rollout

Do one complete lifecycle on one small infrastructure idea. That idea can be
the HIR interpreter contract or a smaller slice of it. The purpose of the
first rollout is to make the transitions visible, not to ship a feature.

Suggested first idea: the HIR interpreter contract with a named ABI line for
slots, function calls, and exported runtime functions. It is currently a
drawing without promises and without an ADR. The first active drawing for this
idea is `docs/plans/abi/execution-ir.md`.

## Guardrails

- An idea dies when it stops being the smallest idea that works. Archive or
  delete the plan rather than keeping a lukewarm plan open.
- Do not write code from a drawing. Promises must exist first.
- Do not change code to match a test that does not carry a promise id.
- Do not reuse a promise id after the compliance check fails.
- Do not treat this document as a substitute for the ABI lifecycle spec.

## Success Criteria

- `docs/specs/abi-lifecycle.md` exists and is linked from `CONTEXT.md`.
- One active idea has a drawing with size, non-goals, and open decisions at
  `docs/plans/abi/execution-ir.md`.
- `docs/adr/` contains no new ADR until that idea reaches signing.
- `tests/` contains no new ABI test until a signed promise exists.
- `memory/plan-debt-status.md` links this plan.
