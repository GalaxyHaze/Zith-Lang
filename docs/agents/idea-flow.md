# Idea Flow

This page documents the route from an idea to signed ABI work. It is the
operational surface for the lifecycle described in `docs/specs/abi-lifecycle.md`.

## When An Idea Appears

Write the idea to `docs/plans/` as one small markdown file, commit it, then
read it back. If the file does not fit in one page, split the idea or shrink
it. Do not discuss ideas only in chat.

The file must contain, in order:

1. `Status: drawing`
2. A one-paragraph problem
3. A size: single unit, module, or system
4. Non-goals
5. Open decisions

## When To Grill

Sharpening happens against the drawing file, not in chat. Edit the drawing with
the questions and answers, then commit. The conversation may add context, but
the file is the primary source.

Use the drawing file for all decisions that change scope. If a decision is
only in chat, it is not a decision.

## When To Block Technical Work

Before technical code or deep architecture work, the drawing must have
promises. A promise is a rule with a seam, a layout obligation, and a named
reversal. Promises live under `## Promises` in the plan.

The technical owner checks facts, computes promises, and reviews them with the
idea owner. No implementation starts from a drawing without promises.

## When To Sign

Write an ADR when the promises survive review. The ADR cites the promise ids
and the plan path. Signing is the commitment that code and tests conform.

## When To Prototype

Prototype when the drawing has an open question that only a runnable answer can
settle. A prototype lives outside the signed contract until its result is
recorded in the plan. It is not a substitute for promises.

Prototype early when the problem is "does this feel right". Do not prototype
when the open question is a fact that research can answer.

## When To Implement

Implement after signing and after drafting conforming tests. The tests carry
promise ids, so a failing test shows exactly which promise is not true.

Implement one slice at a time, from tests to green. Keep the signed promise
ids in the test names or comments.

## When To Archive

Archive a plan when the loop is complete, the idea is rejected, or the idea is
more than one page. Archiving moves the file under `docs/plans/archive/` and
updates the plan index in `memory/plan-debt-status.md`.

Archive with a short note stating why the idea ended. Do not archive lukewarm
work as a pending state; it either dies explicitly or becomes active again as a
new drawing.

## Small Team Rules

- One team member is the idea owner for a plan.
- One team member is the compliance owner for a signed contract.
- A plan is active only when the idea owner returns to it.
- A signed contract changes only through drawing, then promises, then new ADR.
- A technical owner may write promises but never signs without review.

## Exit Criteria

An idea is ready for a real tracker when it has a signed contract and its first
conforming test is red. At that point the idea is a feature plan, not a
drawing.

## From Ideas To Feature Plans

Feature plan task creation runs through ALIN, the alignment of extracted
actions with the idea that produced them. The alignment works at two levels:
the plan header and the plan body.

The header carries the identity of the idea: title, status, size, and the
problem statement. The body carries the substantive content: promises, seams,
layout rows, and reversals. A feature plan must keep those two aligned. If the
header changes scope, the body must change promises. If a promise disappears,
the header must shrink.

Extracted actions are written in three layers:

1. Idea actions: what the idea must stop doing, start doing, or keep true.
2. ABI actions: what the signed contract must prove, test, or archive.
3. Feature plan actions: one task per signed promise id, each with its
   conforming test target.

Extraction is complete when every feature plan action points back to one
promise id and every promise id has at least one feature plan action. An
action that points nowhere is a backlog note, not a feature task. Do not let
broken alignment survive a commit: if an extraction no longer matches the
idea, update the extraction before creating tasks.
