# {AGENT}: {TASK}

## Goal

Split `src/codegen/codegen-emit.cpp` by emission area without changing codegen
output.

## Context

- Plan: monolith splits, third unit.
- Worktree: {REPO}/.awt/{AGENT} (branch awt/{AGENT}).
- Follow `docs/plans/monolith-splits.md`.
- The file is ~1230 lines and mixes expression, call, statement/block, and
  aggregate emission.
- The earlier HIR/sema splits are complete; keep this extraction mechanical and
  behavior-preserving.

## Scope

1. Identify public and private helpers in `codegen-emit.cpp`.
2. Extract expression/call helpers into a focused TU.
3. Extract statement/block helpers into a focused TU.
4. Keep aggregate/layout helpers in a TU named for that responsibility.
5. Re-run CMake and verify LLVM codegen tests.

Out of scope:

- Changes to `frontend-context.cpp` or `compilation-session.cpp` (separate
  scheduler tasks).
- Changing emitted IR, ABI, or runtime behavior.
- New language features.

## Acceptance Criteria

- Public headers and call sites outside the extracted TUs stay unchanged.
- Emitted IR/object output is identical for covered tests.
- No new debug prints.
- Full CTest and codegen tests pass.

## Verification

```bash
cmake --build {REPO}/build -j4
ctest --test-dir {REPO}/build -R 'codegen' --output-on-failure
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

When complete, request merge and wait for the scheduler to advance or write
a `# Status` marker with `end` below it.
