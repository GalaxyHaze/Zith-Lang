# Agent Scheduler Board

## agent1
- current: Reject `for (literal..literal){}` when the loop has no binding, or diagnose it before HIR/codegen
- status: ended

## agent2
- current: Enforce that non-void call expression statements use an explicit `_ = call()` discard
- status: ended

## agent3
- current: Emit E3005 for unchecked nullable-pointer deref/arrow/index/coercion and add flow-sensitive narrowing after `is null`
- status: ended

## agent4
- current: Make `import std/memory` resolve all standalone modules under a shared export prefix
- status: ended

## Pending Debt Review
See `.awt/debts-pending.md`; copy to docs/implementation-debt.md after human review.
