# agent5: Curate Zith-- plans and debt annotations

## Goal

Curate the Zith-- plans and debt documents for the completed monolith splits,
mark the remaining debt correctly, and remove unresolved roadmap conflict
artifacts.

## Context

- Plan: plan and debt curation.
- Worktree: /home/diogo/Zith/.awt/agent5 (branch awt/agent5).
- The compiler main is Zith--, documented in `docs/Zith--.md` and
  `docs/impl-status.md`.
- `docs/roadmap.md` was fixed on `main` at the start of this round; verify
  there are no remaining conflict markers in docs.
- Completed before this round: platform imports, C struct-by-value validation,
  `frontend-context.cpp` split, and `compilation-session.cpp` split.
- Active plan homes: `docs/plans/0.7.0/`, `docs/plans/defer-drop.md`,
  `docs/plans/monolith-splits.md`, `docs/plans/platform-imports.md`, and
  `docs/roadmap.md`.
- Debt home: `docs/implementation-debt.md`; completed items should be marked
  or moved out, and full-Zith-only proposals should be labeled as such.

## Scope

1. Audit `docs/plans/monolith-splits.md` and `docs/implementation-debt.md`:
   remove `src/session/frontend-context.cpp` and
   `src/session/compilation-session.cpp` from the active next-work list and
   link the merged files/line counts.
2. Keep `src/codegen/codegen-emit.cpp` and `src/sema/hir-lower-expr.cpp` as
   remaining candidates with current line counts.
3. Sync `memory/monolith-splits.md` and `memory/plan-debt-status.md` with the
   completed splits; keep every memory file between 200 and 300 lines.
4. Ensure `docs/roadmap.md` has no conflict markers and its dependency graph
   has one correct F-42 line.
5. Audit `docs/plans/0.7.0/` against `docs/impl-status.md`: mark/remove items
   already implemented and proven in Zith--.
6. Ensure full-Zith features remain archived or clearly out of the Zith-- core.
7. Update `docs/impl-status.md` only where a status is demonstrably stale.

Out of scope:

- Implementing compiler features.
- Editing source files outside docs and memory.
- Changing Zith-- language semantics.

## Acceptance Criteria

- Active Zith-- plans no longer claim completed work as next steps.
- Completed splits are documented as merged, not merely pending.
- Full-Zith-only plan material is archived or explicitly scoped out.
- Debt entries distinguish implemented-but-incomplete from intentional
  non-debt and list blocking references.
- No documentation contradicts `docs/Zith--.md` or `docs/impl-status.md`.

## Verification

```bash
rg -n "<<<<<<<|>>>>>>>|=======" /home/diogo/Zith/docs/roadmap.md
rg -n "frontend-context.cpp.*pending|compilation-session.cpp.*pending" /home/diogo/Zith/docs/implementation-debt.md /home/diogo/Zith/docs/plans/monolith-splits.md
rg -n "Status: Working|Status: Parse skipped|Spec only" /home/diogo/Zith/docs/plans/0.7.0 /home/diogo/Zith/docs/roadmap.md | head -80
```

After success:

```bash
cd /home/diogo/Zith/.awt/agent5
/home/diogo/.byteask/skills/agent-worktrees/scripts/awt checkin /home/diogo/Zith "agent5: Curate Zith-- plans and debt annotations"
/home/diogo/.byteask/skills/agent-worktrees/scripts/awt request-merge /home/diogo/Zith "agent5: Curate Zith-- plans and debt annotations"
```

## End Of Task

After requesting merge, do NOT finish this session. Return to the listening
loop: re-read `TASK.md` whenever the scheduler advances this worktree. Execute
each new task as it appears and request merge again after completing it. Stop
only when `TASK.md` contains a `# Status` section with `end` below it.
