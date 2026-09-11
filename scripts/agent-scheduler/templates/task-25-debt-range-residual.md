# {AGENT}: {TASK}

## Goal

Close one small range/increment gap deterministically: preferred candidate is
the residual `1..5`, `1>..5`, `1..<5`, `1>..<5` boundary behavior in loop or
`in` semantics, documented and tested.

## Context

- Plan: debt, range residual and increment operators.
- Worktree: {REPO}/.awt/{AGENT} (branch awt/{AGENT}).
- Follow `docs/implementation-debt.md` under "Outras incompletudes registadas"
  and `docs/impl-status.md` under `range` and `Known Debt`.
- Literal ranges produce `frontend::ExprKind::Range`; integer `for (x in
  int_range)` iterates with implicit step `1`; float ranges are valid with `in`
  and rejected in `for`.

## Scope

1. Pick exactly one gap from this debt list. Recommended: prove and document
   the closed/open boundary behavior for all four range spellings in `for` and
   `in`, and fix the specific mismatch if a test exposes one.
2. If instead `++`/`--` is chosen, document that Zith-- intentionally keeps them
   unimplemented and covers the decision, without adding operators.
3. Add focused frontend/sema/HIR tests for the selected range/boundary forms,
   including empty and exclusive-bound cases.
4. Update debt and status text to mark the selected sub-item resolved.

Owned files for implementation:

- Range parsing only if a boundary bug is found:
  `src/frontend/frontend-expr.cpp`.
- Range sema: `src/sema/sema-control.cpp` and `src/sema/sema-index.cpp` only if
  the chosen boundary test needs a sema fix; prefer tests first.
- Range HIR lowering: `src/sema/hir-lower-block.cpp`, but do not edit
  `src/sema/hir-lower-expr.cpp`; that file is owned by the opaque registry
  debt task in this wave.
- Tests: `tests/test-optional-slice.cpp`; do not add new executables.
- Docs: `docs/implementation-debt.md`, `docs/impl-status.md`.
  Edit only the section/line for this item in each shared file.

Out of scope:

- General loop syntax changes; do not edit `src/frontend/frontend-stmt.cpp`.
- Formatter changes; do not edit `src/formatter/*`.
- Numeric narrowing overflow; do not edit `src/sema/sema-cast-coerce.cpp`.
- Adding `++`/`--` operators unless the manifest was later edited to require a
  deliberate language extension.
- Opaque, cache, NRA, and trait/interface import conformance work.

## Acceptance Criteria

- Every tested range spelling has documented boundary semantics.
- The selected gap is either fixed with a regression test or explicitly
  documented as intended for Zith--.
- No unrelated loop or index behavior regresses.

## Verification

```bash
cmake --build {REPO}/build -j4
ctest --test-dir {REPO}/build -R 'range|control|codegen|index' --output-on-failure
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
