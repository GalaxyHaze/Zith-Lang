# {AGENT}: {TASK}

## Goal

Split `src/session/frontend-context.cpp` by responsibility without changing
compiler behavior.

## Context

- Plan: monolith splits, first unit.
- Worktree: {REPO}/.awt/{AGENT} (branch awt/{AGENT}).
- Follow the execution contract in `docs/plans/monolith-splits.md`.
- The current file is ~1800 lines and mixes fingerprinting, source catalogs,
  import requests, module cache bookkeeping, scoped symbol compilation, and
  module analysis.
- Keep the public `FrontendContext` API and existing headers stable.

## Scope

1. Extract `ContentFingerprint` and source-catalog helpers into a focused TU,
   if they are not already cleanly separable.
2. Extract module cache/executor bookkeeping into a focused TU.
3. Extract import/module analysis or symbol/import resolution into focused TUs.
4. Re-run CMake after adding `.cpp` files because the root glob needs to see
   them.
5. Run formatting and full tests; report any behavior-preserving call-site
   change.

Out of scope:

- `src/session/compilation-session.cpp` and `src/codegen/codegen-emit.cpp`
  (separate scheduler tasks).
- Adding language features or new diagnostics.
- Cleaning the dirty master worktree or user changes.

## Acceptance Criteria

- Public headers and call sites outside the extracted TUs stay unchanged.
- The file count and line counts match the plan where practical.
- No new debug prints and no `std::cerr`/`std::printf` in the diff.
- Focused and full CTest pass with the same status as before the split.

## Verification

```bash
cmake --build {REPO}/build -j4
ctest --test-dir {REPO}/build -R 'frontend' --output-on-failure
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
