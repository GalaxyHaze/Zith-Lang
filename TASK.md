# agent5: Curate Zith-- plans and debt annotations

## Goal

Curate the Zith-- plans and debt documents: remove completed items from active
plans, archive full-Zith-only material, and write precise annotations of what
remains as debt.

## Context

- Plan: plan and debt curation.
- Worktree: /home/diogo/Zith/.awt/agent5 (branch awt/agent5).
- The compiler main is Zith--, documented in `docs/Zith--.md` and
  `docs/impl-status.md`.
- Active plan homes: `docs/plans/0.7.0/`, `docs/plans/defer-drop.md`,
  `docs/plans/monolith-splits.md`, `docs/plans/platform-imports.md`, and
  `docs/roadmap.md`.
- Debt home: `docs/implementation-debt.md`; completed items should be marked
  or moved out, and full-Zith-only proposals should be labeled as such.

## Scope

1. Audit `docs/plans/0.7.0/` against `docs/impl-status.md`: mark/remove items
   already implemented and proven in Zith--.
2. Ensure full-Zith features remain archived or clearly out of the Zith-- core.
3. Update `docs/implementation-debt.md` with precise status: real debt vs
  intentional non-debt; add missing annotations for gaps.
4. Update `docs/impl-status.md` only where a status is demonstrably stale.
5. Add a short `memory/` note if a non-obvious plan/debt relationship is
  discovered.
6. Keep every memory file between 200 and 300 lines and consistent with the
  project memory conventions.

Out of scope:

- Implementing compiler features.
- Editing source files outside docs and memory, except where a stale reference
  must be fixed.
- Changing Zith-- language semantics.

## Acceptance Criteria

- Active Zith-- plans no longer claim completed work as next steps.
- Full-Zith-only plan material is archived or explicitly scoped out.
- Debt entries distinguish implemented-but-incomplete from intentional
  non-debt and list blocking references.
- No documentation contradicts `docs/Zith--.md` or `docs/impl-status.md`.

## Verification

```bash
rg -n "Status: Working|Status: Parse skipped|Spec only" docs/plans/0.7.0 docs/roadmap.md | head -80
rg -n "TODO|Next step|Pending" docs/implementation-debt.md | head -80
cat docs/impl-status.md | head -40
```

After success:

```bash
cd /home/diogo/Zith/.awt/agent5
/home/diogo/.byteask/skills/agent-worktrees/scripts/awt checkin /home/diogo/Zith "agent5: Curate Zith-- plans and debt annotations"
/home/diogo/.byteask/skills/agent-worktrees/scripts/awt request-merge /home/diogo/Zith "agent5: Curate Zith-- plans and debt annotations"
```

## End Of Task

When complete, request merge and wait for the scheduler to advance or write
a `# Status` marker with `end` below it.
