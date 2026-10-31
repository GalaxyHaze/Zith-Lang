# Contextual (Expected-Result) Generic Inference

The generic instantiator can now bind a function's type parameters from the
**expected result type** at the call site, so parameters that appear only in the
return type no longer need an explicit `<...>` list. This is what lets
`Ok(7)`, `Err(e)`, and `new(3)` infer their arguments from a typed position.

## Where It Lives

- `src/comptime/generic-instantiate.{hpp,cpp}`: `resolveArgs` takes
  `expected_result` and `const std::vector<bool> &weak_args`; `resolveTypes`
  takes `declared_result`, `expected_result`, `weak_args`. After argument
  unification it runs `unify(declared_result, expected_result, weak=false)`.
- `src/sema/sema-modern.hpp`: member `expectedResultType_`, plus
  `inferCall(id, expected_result)` and `inferExprExpected(id, expected)`.
  `inferMethodCall` and `resolveStructMethodCall` also take `expected_result`
  and forward it to `resolveArgs`, so a generic *method* with a result-only
  parameter (`fn make<T>(self): ?*T`) binds the same way as a free function.
- `src/sema/sema-expr.cpp`: `inferExpr` captures and clears
  `expectedResultType_` at entry and forwards it to `inferCall`.
  `inferExprExpected` sets/restores the member.
- `src/sema/sema-control.cpp`: binding statements pass the annotation type as
  the expected result.
- `src/sema/sema-state.cpp`: `checkReturnStatement` / `inferReturn` pass
  `currentReturnType_`.
- `src/sema/sema-call.cpp`: builds `weak_args` (helper `weakLiteral`, recurses
  through unary `-`).

## Weak vs Strong Bindings

An unsuffixed numeric literal binds a parameter **weakly**. A later strong
binding (the expected result) overrides the weak binding, so
`let r: Result<u32, u32> = Ok(7);` resolves `T = u32`, not `T = i32`. A strong
argument that mismatches the expected result is still rejected (`E3011`).

## Scope / Limits

- Only a **direct call** benefits: `inferExprExpected` sets the expected type
  for the call it wraps. Nested sub-expressions do not currently forward it.
- Call **arguments** are not yet used as expected-result hints for typed
  parameter positions (only binding annotations and `return` feed the hint).
- `resolveStruct` passes `{}` weak args.

## Tests

- `tests/test-enum-union-generics.cpp`:
  `test_result_only_generic_infers_from_context`,
  `test_strong_argument_overrides_weak_literal`,
  `test_method_result_only_generic_infers_from_context`.
- `tests/test-codegen.cpp`: `test_imported_result_runtime`.
