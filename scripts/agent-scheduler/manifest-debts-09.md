# Multitask Plan Manifest

> Zith-- only. The compiler main is Zith--; full-Zith features are archived or
> out of scope. This manifest is consumed by
> `scripts/agent-scheduler/scheduler.py`.

## agent1 - Debt: nominal type surface

Plan
- Decide and implement/doco the Zith-- syntax for nominal `type Name = T`

Task files
- scripts/agent-scheduler/templates/task-17-debt-nominal-type.md

Dependencies: none. Owns nominal parser/sema/lowering; must not touch sema-cast-coerce.cpp.

## agent2 - Debt: NRA slice

Plan
- Make one NRA/ownership slice deterministic and document the residual boundary

Task files
- scripts/agent-scheduler/templates/task-18-debt-nra-partial.md

Dependencies: none. Owns NRA facts, HirAttrs, narrowing helpers, and pipeline glue; must not touch formatter or trait resolver.

## agent3 - Debt: opaque canonical registry

Plan
- Make bare opaque tag stability an explicit cross-module registry/cache contract

Task files
- scripts/agent-scheduler/templates/task-19-debt-opaque-registry.md

Dependencies: none. Owns cache/registry/persistent hydration; must not touch sema-cast-coerce.cpp or frontend-symbol-resolution.cpp.

## agent4 - Debt: C interop residual

Plan
- Extend or explicitly document one validated C record ABI slice

Task files
- scripts/agent-scheduler/templates/task-20-debt-cinterop-residual.md

Dependencies: none. Owns cinterop record layout and tests; must not touch native-link or persistent-cache.

## agent5 - Debt: trait/interface import conformance

Plan
- Reproduce and fix imported trait/interface conformance instability in populated workdirs

Task files
- scripts/agent-scheduler/templates/task-21-debt-trait-import-conformance.md

Dependencies: none. Owns import resolution and sema conformance; must not touch ast-lowerer, sema-cast-coerce, or cache registry.

## agent6 - Debt: ProjectConfig + Options merge

Plan
- Remove repeated ProjectConfig + Options concatenation with one merge helper

Task files
- scripts/agent-scheduler/templates/task-22-debt-project-config-merge.md

Dependencies: none. Owns session/native-link merge logic; must not touch cache or NRA files.

## agent7 - Debt: formatter for loops

Plan
- Make `for (cond)` round-trip as `for (cond)`, not `while`

Task files
- scripts/agent-scheduler/templates/task-23-debt-formatter-for.md

Dependencies: none. Owns loop parse representation and formatter; must not touch frontend-expr or sema-cast-coerce.

## agent8 - Debt: narrowing overflow

Plan
- Add overflow diagnostics for narrowing numeric casts and literal adaptation

Task files
- scripts/agent-scheduler/templates/task-24-debt-narrowing-overflow.md

Dependencies: none. Owns sema-cast-coerce and int-literal helpers; must not touch parser/formatter.

## agent9 - Debt: range residual

Plan
- Close one small range/increment gap with tests and a documented decision

Task files
- scripts/agent-scheduler/templates/task-25-debt-range-residual.md

Dependencies: none. Owns range parsing/sema/HIR; must not touch loop statement parsing, formatter, or sema-cast-coerce.
