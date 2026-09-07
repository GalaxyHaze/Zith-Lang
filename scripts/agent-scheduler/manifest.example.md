# Multitask Plan Manifest

> Zith-- only. The compiler main is Zith--; full-Zith features are archived or
> out of scope. This manifest is consumed by
> `scripts/agent-scheduler/scheduler.py`.

## agent1 - Monolith: codegen-emit

Plan
- Split src/codegen/codegen-emit.cpp by responsibility

Task files
- scripts/agent-scheduler/templates/task-07-monolith-codegen.md

Dependencies: none. Keep the codegen split isolated from sema/cache work.

## agent2 - Debt: variadic tail plan

Plan
- Centralize variadic-slice tail decisions in sema

Task files
- scripts/agent-scheduler/templates/task-08-variadic-call-plan.md

Dependencies: none. Do not split files owned by agent1 or agent3.

## agent3 - Debt: HIR initializers

Plan
- Give HIR expression nodes complete default initialization

Task files
- scripts/agent-scheduler/templates/task-09-hir-initializers.md

Dependencies: none. Keep changes to `src/hir/hir-expr.hpp` and builder call
sites that initialize the new defaults.

## agent4 - Debt: opaque cache hydration

Plan
- Rehydrate and import bare opaque tags across cache/module boundaries

Task files
- scripts/agent-scheduler/templates/task-10-opaque-cache-hydration.md

Dependencies: none. It may touch `persistent-cache.cpp` and lowering, but must
not take the monolith-split work from other agents.

## agent5 - Plan and debt curation

Plan
- Curate Zith-- plans and debt annotations

Task files
- scripts/agent-scheduler/templates/task-11-curate-plans-debts.md

Dependencies: docs-only; can run in parallel with agent1-4.
