# Plan: Type Inference for `fn` + Implicit Returns

## Overview

Two related features to make Zith more ergonomic:

1. **Return type inference**: When `-> Type` is omitted on `fn`, infer the return type from the function body
2. **Implicit return**: The last expression in a function body is automatically returned without needing `return`

## User Choices

- **Scope**: Both features implemented together
- **Recursion**: Prohibited without explicit `-> Type` annotation (detect and emit error)
- **Control flow**: Last-statement-only (MVP — no exhaustiveness checking)

## Current State

| Aspect | Current | Target |
|--------|---------|--------|
| Return type annotation | Required (defaults to `void`) | Optional, inferred from body |
| Return statement | Explicit `return` required | Last expression implicitly returned |
| `ctx.current_return` | Set from `fn->return_type` (line 1216) | Set after body analysis when inferring |

## Files to Modify

| File | Changes |
|------|---------|
| `impl/parser/parser_sema.cpp` | `sema_stmt` → returns `Type`; two-pass inference loop; `inferred_returns` side table; block returns last type; recursion detection |
| `impl/cli/runtime_interpreted/runtime_interpreted.cpp` | `exec_block` stores/returns last expression value; `exec_stmt` returns expression values |
| `tests/test_parser.cpp` | Add test cases |

## Implementation Phases

### Phase 1: Implicit Return — `sema_stmt` returns `Type`

- Change `sema_stmt` from `void` to `Type` (returns type of last statement)
- `ZITH_NODE_BLOCK`: return the type of the last statement in the block
- Expression statements: return their type
- `ZITH_NODE_RETURN`: return `SemaType::Void` (explicit return suppresses implicit)
- `ZITH_NODE_IF`/`ZITH_NODE_FOR`: return `SemaType::Void` for now

### Phase 2: Return Type Inference — Two-pass Analysis

**Pass 1** (before main sema loop): For each `FUNC_DECL` without `return_type`:
- Create temp `SemaContext`, define params, analyze body
- Store inferred type in `ctx.inferred_returns[fn]`
- Detect recursion: if function calls itself → error "recursive function requires explicit return type annotation"

**Pass 2** (existing loop): Set `ctx.current_return` from either `fn->return_type` or `inferred_returns` map

### Phase 3: Interpreter Updates

- `exec_block` returns the last statement's value even without explicit `return`
- `exec_stmt` returns expression value for expression statements (currently returns void)
- Functions called via `eval_call` already return `exec_block` result — no change needed there

### Phase 4: Diagnostics & Edge Cases

- **Divergent branches**: Only check last statement type (MVP)
- **Explicit + implicit**: Trailing expression is implicit return, explicit `return` is early exit
- **Void functions**: No trailing expression or trailing expression of type `void`

## Example After Implementation

```zith
// Inferred return type: int, implicit return
fn add(a: i32, b: i32) {
    a + b
}

// Explicit return type still works
fn sub(a: i32, b: i32) -> i32 {
    return a - b;
}

// Void function (no trailing expression)
fn greet() {
    println("hello")
}

// Error: recursive without explicit type
fn fib(n: i32) {
    if n < 2 { n } else { fib(n - 1) + fib(n - 2) }
}
// → "recursive function 'fib' requires explicit return type annotation"
```
