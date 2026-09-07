# {AGENT}: {TASK}

## Goal

Make HIR expression node variants fully initialized by default so moving or
extending their builders cannot introduce indeterminate members.

## Context

- Plan: implementation debt, HIR node safety.
- Worktree: {REPO}/.awt/{AGENT} (branch awt/{AGENT}).
- Follow `docs/implementation-debt.md`, section "HIR nodes sem initializers
  completos".
- `cpp_check` reports `uninitMemberVarNoCtor` in `src/hir/hir-expr.hpp`.
- Nodes are constructed with aggregate initialization in `src/sema/`;
  uninitialized scalar/id fields can escape when a builder moves.

## Scope

1. Enumerate every field in `src/hir/hir-expr.hpp` that lacks a default
   initializer, including numeric/boolean/id fields and `memory::DynArray`
   members of non-constructor variants.
2. Add default member initializers or dedicated constructors so each
   `HirExpr` alternative is well-defined without exceptions or RTTI.
3. Preserve arena-backed ownership and the existing aggregate construction
   pattern where practical.
4. Verify with `cpp_check` that the reported `uninitMemberVarNoCtor` findings
   are gone.
5. Run focused sema/HIR/codegen tests and the full suite.
6. Update `docs/implementation-debt.md` when the pattern is closed.

Out of scope:

- Changing HIR semantics or layout.
- Adding default memory initialization to plain data that already has
  meaningful `= kInvalid...` defaults.
- Full-Zith features.

## Acceptance Criteria

- Every HIR expr variant is fully initialized by default construction.
- No `uninitMemberVarNoCtor` remains for `src/hir/hir-expr.hpp`.
- Existing tests pass without behavior change.
- No new runtime/diagnostic behavior is introduced.

## Verification

```bash
cmake --build {REPO}/build -j4
ctest --test-dir {REPO}/build -R 'sema|codegen' --output-on-failure
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
