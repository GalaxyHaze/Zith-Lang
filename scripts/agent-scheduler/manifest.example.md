# Multitask Plan Manifest

> Zith-- only. The compiler main is Zith--; full-Zith features are archived or
> out of scope. This manifest is consumed by
> `scripts/agent-scheduler/scheduler.py`.

## agent1 - Platform imports

Plan
- Platform-specific imports (`foo.<arch>.<os>.zith`)

Task files
- scripts/agent-scheduler/templates/task-01-platform-imports.md

Dependencies: none. Keep `src/session/frontend-context.cpp` edits isolated from
agent2's compilation-session split.

## agent2 - Monolith: frontend-context

Plan
- Split src/session/frontend-context.cpp by responsibility

Task files
- scripts/agent-scheduler/templates/task-02-monolith-frontend-context.md

Dependencies: none for this first monolith unit. Do not touch platform-import
ownership of the same resolver file without coordinating through the master.

## agent3 - Monolith: compilation-session

Plan
- Split src/session/compilation-session.cpp by responsibility
- Split src/codegen/codegen-emit.cpp by responsibility

Task files
- scripts/agent-scheduler/templates/task-03-monolith-compilation-session.md
- scripts/agent-scheduler/templates/task-04-monolith-codegen.md

Dependencies: none. This agent completes the compilation-session split first,
then the codegen split in the same worktree. Keep both files isolated from
agent1/agent2.

## agent4 - C ABI struct-by-value

Plan
- Validated simple-record C struct-by-value ABI

Task files
- scripts/agent-scheduler/templates/task-05-c-abi-struct-by-value.md

Dependencies: none for the C binder task; it may require codegen awareness but
must not wait on the codegen monolith split.

## agent5 - Plan and debt curation

Plan
- Curate Zith-- plans and debt annotations

Task files
- scripts/agent-scheduler/templates/task-06-curate-plans-debts.md

Dependencies: docs-only; can run in parallel with agent1-4.
