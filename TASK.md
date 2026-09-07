# agent1: Split src/codegen/codegen-emit.cpp by responsibility

## Goal

Split `src/codegen/codegen-emit.cpp` by emission area without changing codegen
output.

## Context

- Plan: monolith splits, remaining first pass.
- Worktree: /home/diogo/Zith/.awt/agent1 (branch awt/agent1).
- Follow `docs/plans/monolith-splits.md`.
- `src/codegen/codegen-emit.cpp` is ~1264 lines and mixes expression/call
  helpers, statement/block emission, and aggregate/lvalue emission.
- `src/session/frontend-context.cpp` and `src/session/compilation-session.cpp`
  were already split and are not part of this task.

## Scope

1. Identify public and private helpers in `src/codegen/codegen-emit.cpp`.
2. Extract expression/call helpers into a focused TU under `src/codegen/`.
3. Extract statement/block helpers into a focused TU.
4. Keep aggregate/layout helpers in a TU named for that responsibility.
5. Re-run CMake after adding `.cpp` files because the root build uses a glob.
6. Run `fmt`/`fmt-check` on touched files.

Out of scope:

- Changing emitted IR, ABI, runtime behavior, or public headers/call sites
  outside the extracted TUs.
- Splitting `src/sema/hir-lower-expr.cpp` or `src/frontend/frontend-expr.cpp`.
- New language features or diagnostics.

## Acceptance Criteria

- Public headers and call sites outside the extracted TUs stay unchanged.
- Emitted IR/object output is identical for covered tests.
- No new debug prints and no `std::cerr`/`std::printf` in the diff.
- Focused and full CTest pass with the same status as before the split.

## Verification

```bash
cmake -S /home/diogo/Zith -B /home/diogo/Zith/build
cmake --build /home/diogo/Zith/build -j4
ctest --test-dir /home/diogo/Zith/build -R 'codegen' --output-on-failure
cmake --build /home/diogo/Zith/build --target fmt-check
ctest --test-dir /home/diogo/Zith/build --output-on-failure
```

After success:

```bash
cd /home/diogo/Zith/.awt/agent1
/home/diogo/.byteask/skills/agent-worktrees/scripts/awt checkin /home/diogo/Zith "agent1: Split src/codegen/codegen-emit.cpp by responsibility"
/home/diogo/.byteask/skills/agent-worktrees/scripts/awt request-merge /home/diogo/Zith "agent1: Split src/codegen/codegen-emit.cpp by responsibility"
```

## End Of Task

After requesting merge, do NOT finish this session. Return to the listening
loop: re-read `TASK.md` whenever the scheduler advances this worktree. Execute
each new task as it appears and request merge again after completing it. Stop
only when `TASK.md` contains a `# Status` section with `end` below it.
