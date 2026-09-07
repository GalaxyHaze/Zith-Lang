# {AGENT}: {TASK}

## Goal

Split `src/session/compilation-session.cpp` by responsibility without changing
pipeline behavior.

## Context

- Plan: monolith splits, second unit.
- Worktree: {REPO}/.awt/{AGENT} (branch awt/{AGENT}).
- Follow `docs/plans/monolith-splits.md`.
- The file is ~2000 lines and mixes `CompilationSession` orchestration,
  persistent cache, native C compilation, link/run, and CLI-facing helpers.
- Keep `CompilationSession` as the orchestrator; extract object cache, native
  link/run, and cache persistence into TUs named for those responsibilities.

## Scope

1. Extract object-cache and persistent-cache helpers from
   `compilation-session.cpp`.
2. Extract native C compile/link/run helpers, keeping the public
   `CompilationSession` API stable.
3. Keep pipeline stage orchestration in `compilation-session.cpp`.
4. Re-run CMake after adding `.cpp` files.
5. Run focused build and full tests.

Out of scope:

- `src/session/frontend-context.cpp` and `src/codegen/codegen-emit.cpp`
  (separate scheduler tasks).
- Changing compiler semantics, cache format, or CLI behavior.
- Cleaning or reverting master user changes.

## Acceptance Criteria

- Public headers and call sites outside the extracted TUs stay unchanged.
- Pipeline order and cache behavior are identical.
- No new debug prints.
- Focused and full CTest pass.

## Verification

```bash
cmake --build {REPO}/build -j4
ctest --test-dir {REPO}/build -R 'cache|cli' --output-on-failure
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
