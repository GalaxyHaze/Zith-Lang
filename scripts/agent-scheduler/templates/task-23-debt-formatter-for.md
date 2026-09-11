# {AGENT}: {TASK}

## Goal

Make the formatter round-trip `for (cond) { }` as `for (cond) { }`, not as a
deprecated `while`.

## Context

- Plan: debt, formatter reprints `for (cond)` as `while`.
- Worktree: {REPO}/.awt/{AGENT} (branch awt/{AGENT}).
- Follow `docs/implementation-debt.md` under "Outras incompletudes registadas"
  and `docs/impl-status.md` under `Known Debt`.
- `parseFor()` currently creates `ExprKind::While` for the conditional/infinite
  forms, so the formatter prints those nodes as `while`.

## Scope

1. Resolve the parsing representation so the formatter can distinguish a source
   `for (cond)`/`for {}` from a source `while`.
2. Preserve sema/HIR lowering for all loop forms; the pipeline may continue to
   lower both into the same CFG.
3. Keep `while` parseable with its existing deprecation warning and make
   formatting output non-deprecated spelling only when source used `for`.
4. Add formatter round-trip tests for conditional, infinite, labeled, and
   optional-condition `for` forms.

Owned files for implementation:

- Parse representation: `src/frontend/frontend-stmt.cpp`,
  `src/frontend/frontend.hpp`, `src/frontend/frontend-printer.cpp`.
- Formatter: `src/formatter/fmt-visitor.cpp`.
- Tests: `tests/test-formatter.cpp`; do not add new executables.
- Docs: `docs/implementation-debt.md`, `docs/impl-status.md`.
  Edit only the section/line for this item in each shared file.

Out of scope:

- Changing `while` semantics or deprecation policy.
- Range/increment work; this task must not change
  `src/frontend/frontend-expr.cpp` or the range lowering in
  `src/sema/hir-lower-expr.cpp`; the range debt task owns range files.
- Numeric narrowing diagnostics; do not edit `src/sema/sema-cast-coerce.cpp`.
- Sema/HIR ownership in this wave; if the loop representation needs lowering
  awareness, record it as a follow-up instead of editing `src/sema/*` or
  `src/hir/*`.

## Acceptance Criteria

- `zithc fmt` on `for (cond) { }` is stable on a second run and does not print
  `while`.
- Source `while` continues to parse and emits the same deprecation warning.
- Loop runtime behavior is unchanged.

## Verification

```bash
cmake --build {REPO}/build -j4
ctest --test-dir {REPO}/build -R 'formatter|codegen|control|loop' --output-on-failure
cmake --build {REPO}/build --target fmt-check
ctest --test-dir {REPO}/build --output-on-failure
```

After success:

```bash
cd {REPO}/.awt/{AGENT}
{AWT_SKILL_DIR}/scripts/awt checkin {REPO} "{AGENT}: {TASK}"
{AWT_SKILL_DIR}/scripts/awt request-merge {REPO} "{AGENT}: {TASK}"
```

## End Of Task

After requesting merge, do NOT finish this session. Return to the listening
loop: re-read `TASK.md` whenever the scheduler advances this worktree. Execute
each new task as it appears and request merge again after completing it. Stop
only when `TASK.md` contains a `# Status` section with `end` below it.
