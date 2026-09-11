# {AGENT}: {TASK}

## Goal

Add deterministic overflow diagnostics for narrowing numeric casts and literal
adaptation to a narrower target type.

## Context

- Plan: debt, no overflow check on narrowing conversions.
- Worktree: {REPO}/.awt/{AGENT} (branch awt/{AGENT}).
- Follow `docs/implementation-debt.md` under "Outras incompletudes registadas"
  and `docs/impl-status.md` under `Known Debt` and `as` cast.
- Numeric `as` casts currently lower without checking that the value fits the
  target width/sign.

## Scope

1. Define exactly which `as` numeric pairs are compile-time checkable in Zith--:
   constant operands, literal operands, and same-width sign changes should be
   preferred; leave variable-width runtime checks for a later explicit
   decision.
2. Add diagnostics when a constant/literal narrowing cast cannot fit the target.
3. Add accepted and rejected focused tests for signed/unsigned and narrower
   integer targets, and document rule text in the debt note.
4. Keep the current general `as` rules and codegen shape for non-narrowing
   casts.

Owned files for implementation:

- Cast/sema logic: `src/sema/sema-cast-coerce.cpp`.
- Literal helper if shared: `src/support/int-literal.hpp`,
  `src/support/int-literal.cpp` if it exists.
- Literal expression inference glue if needed:
  `src/sema/sema-expr.cpp`.
- HIR/codegen only if a new checked cast shape is required; otherwise do not
  change lowering.
- Tests: `tests/test-codegen.cpp`; do not add new executables.
- Docs: `docs/implementation-debt.md`, `docs/impl-status.md`.
  Edit only the section/line for this item in each shared file.

Out of scope:

- Runtime overflow checks for variable-width arithmetic and casts.
- Opaque/nominal cast logic beyond keeping it intact.
- Changes to `src/frontend/ast-lowerer.cpp`, `src/frontend/frontend-decl.cpp`,
  `src/frontend/frontend-expr.cpp`, and `src/formatter/*`.
- Sema ownership files beyond `src/sema/sema-cast-coerce.cpp` and
  `src/sema/sema-expr.cpp`; the NRA slice owns sema-assign/narrowing files and
  the range slice owns sema-control/index.

## Acceptance Criteria

- A narrowing literal/constant that cannot fit the target reports a clear
  diagnostic and does not silently truncate.
- Existing valid numeric casts and pointer/opaque casts pass unchanged.
- The accepted surface is documented.

## Verification

```bash
cmake --build {REPO}/build -j4
ctest --test-dir {REPO}/build -R 'cast|codegen|literal|narrow' --output-on-failure
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
