# Multitask Plan Manifest

> Zith-- only. The compiler main is Zith--; full-Zith features are archived or
> out of scope. This manifest is consumed by
> `scripts/agent-scheduler/scheduler.py`.

## agent1 - Debt: bodyless literal range for crashes

Plan
- Reject `for (literal..literal){}` when the loop has no binding, or diagnose it before HIR/codegen

Task files
- scripts/agent-scheduler/templates/task-26-debt-range-literal-for-bodyless.md

Dependencies: none. Owns range `for` sema/HIR and defensive codegen guard only; must not touch loop statement parsing, formatter, or sema-cast-coerce.

## agent2 - Debt: discard-only expression statements

Plan
- Enforce that non-void call expression statements use an explicit `_ = call()` discard

Task files
- scripts/agent-scheduler/templates/task-27-debt-nodiscard-default.md

Dependencies: none. Owns expression-statement sema and the `_ =` discard path; must not add a `nodiscard` keyword, function kind, or declaration field.

## agent3 - Debt: pointer narrowing

Plan
- Emit E3005 for unchecked nullable-pointer deref/arrow/index/coercion and add flow-sensitive narrowing after `is null`

Task files
- scripts/agent-scheduler/templates/task-28-debt-pointer-narrowing.md

Dependencies: none. Owns pointer sema/index/arrow and docs; must not touch sema-cast-coerce.cpp or range files.

## agent4 - Debt: export namespace fanout

Plan
- Make `import std/memory` resolve all standalone modules under a shared export prefix

Task files
- scripts/agent-scheduler/templates/task-29-debt-export-namespace-fanout.md

Dependencies: none. Owns frontend symbol resolution/module analysis/tests; must not touch sema files.
