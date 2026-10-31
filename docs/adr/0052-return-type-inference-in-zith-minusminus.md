# Return Type Inference in Zith--

Status: accepted for the Zith-- iteration.

A Zith-- function written without a return annotation infers its return type
from its body. Recursion is forbidden in a function whose return type is
inferred, so inference stays a single forward pass.

## Context

`docs/05-functions.md` already promises the inferred form:

```zith
fn add(a: i32, b: i32): i32 { a + b }   // explicit type, implicit return
fn add(a: i32, b: i32)      { a + b }   // inferred type
```

The compiler does not implement the second form. `PerModuleSema` assigns a
missing return type the `void` type in `src/sema/sema-decl.cpp`, so
`fn add(a: i32, b: i32) { a + b }` reports `E2026` (void body discards a value)
and `E3001` (return type mismatch). The behavior and the documentation diverge.

The pieces for the inference already exist: the expected-type channel
(`inferExprExpected`, `expectedResultType_`, `currentReturnType_`), the flow
analysis (`exprAlwaysTerminates`, `statementAlwaysTerminates`), and the
`E3002 CannotInfer` diagnostic for the ambiguous case.

## Decision

A function declaration with no written return type infers its return type from
its body. The inferred type is the type of the tail expression when the body
produces a value, unified with the type of every `return` in the body. When the
body produces no value, the function returns `void`, as today.

Recursion is forbidden in a function whose return type is inferred. A function
with a written return type may still be recursive. Direct recursion (a function
calls itself) and indirect recursion (a cycle among functions that all infer
their return type) are both rejected, with a diagnostic that names the missing
annotation. This removes the only case that would need a placeholder type
variable seeded before the body is checked, so inference is a single pass.

The `union` and `dyn` result types keep the existing rule: they cannot be
inferred without an explicit hint.

## Considered Options

- **Allow recursion and use a placeholder type variable.** Deferred. It is the
  textbook unification approach, but it is not needed for the common case and
  it interacts with the optional coercions and `coerceValue`. Banning recursion
  buys a simpler and more predictable rule.
- **Infer only a single tail expression, reject multiple `return`s.** Rejected
  as too narrow: `if`/`else` and `when` bodies with several returns are the
  cases users expect to work.
- **Leave the documentation as is and keep the compiler rejecting.** Rejected:
  a documented form that the compiler refuses is a defect, not a feature gate.

## Consequences

- The documented inferred-return form in `docs/05-functions.md` becomes real,
  closing the doc and implementation divergence.
- The recursion ban is a new rule that needs a call-graph cycle check over
  functions with inferred returns. A function that wants recursion keeps it by
  writing the return type, so no expressiveness is lost, only inference.
- `docs/impl-status.md` and `docs/Zith---implementation.md` must describe the
  rule, including the recursion ban and the `E3002` case.
- This is the prerequisite for the inferred union return in
  [ADR-0053](0053-inferred-unions-in-zith-minusminus.md), which reuses the same
  expected-type channel.
