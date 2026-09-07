# agent2: Centralize variadic-slice tail decisions in sema

## Goal

Centralize variadic-tail decision logic in a `VariadicCallPlan` produced by
sema and consumed by lowering, removing the duplicated local rules.

## Context

- Plan: implementation debt, variadic slice calls.
- Worktree: /home/diogo/Zith/.awt/agent2 (branch awt/agent2).
- Follow `docs/implementation-debt.md`, section "Duplicação de variadic tail
  logic".
- Today the explicit-slice vs auto-collect decision is recomputed in
  `src/sema/sema-call.cpp`, `src/sema/sema-method.cpp`,
  `src/sema/sema-control.cpp`, `src/sema/sema-state.cpp`,
  `src/sema/hir-lower-call.cpp`, and `src/sema/hir-lower-stmt.cpp`.
- The HIR lowering must consume the sema decision instead of reimplementing it.

## Scope

1. Define `VariadicCallPlan` in `src/sema/` with the fields needed by lowering:
   callee slice parameter index, explicit-slice flag, auto-collect flag,
   resolved slice type, and the lower span.
2. Populate the plan during sema call resolution for direct calls, method
   calls, `dyn` calls, statement calls, and state transitions that use a
   variadic slice tail.
3. Store the plan on the typed call/snapshot or the resolved binding so the
   lowering stages can read it without re-derived hints.
4. Update `src/sema/hir-lower-call.cpp` and `src/sema/hir-lower-stmt.cpp` to
   consume the plan and keep existing behavior and diagnostics.
5. Add focused tests that prove explicit `[]T` and collected tail calls lower
   identically for free functions, methods, `dyn` calls, and state calls.
6. Update `docs/implementation-debt.md` when the duplication is closed.

Out of scope:

- Changing variadic function (C `...`) behavior.
- Changing overload selection or arity diagnostics.
- Full-Zith features.

## Acceptance Criteria

- The explicit-slice vs auto-collect decision is produced in one sema
  authority and consumed mechanically by lowering.
- Existing Zith-- variadic slice tests pass unchanged.
- No behavior or diagnostic regression in calls, methods, state transitions,
  or `dyn` dispatch.

## Verification

```bash
cmake --build /home/diogo/Zith/build -j4
ctest --test-dir /home/diogo/Zith/build -R 'sema|codegen|cinterop' --output-on-failure
cmake --build /home/diogo/Zith/build --target fmt-check
ctest --test-dir /home/diogo/Zith/build --output-on-failure
```

After success:

```bash
cd /home/diogo/Zith/.awt/agent2
/home/diogo/.byteask/skills/agent-worktrees/scripts/awt checkin /home/diogo/Zith "agent2: Centralize variadic-slice tail decisions in sema"
/home/diogo/.byteask/skills/agent-worktrees/scripts/awt request-merge /home/diogo/Zith "agent2: Centralize variadic-slice tail decisions in sema"
```

## End Of Task

After requesting merge, do NOT finish this session. Return to the listening
loop: re-read `TASK.md` whenever the scheduler advances this worktree. Execute
each new task as it appears and request merge again after completing it. Stop
only when `TASK.md` contains a `# Status` section with `end` below it.
