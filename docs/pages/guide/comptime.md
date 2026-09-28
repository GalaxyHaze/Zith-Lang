---
id: guide-comptime
title: Comptime
section: Language Guide
output: guide/D-comptime.html
aliases: language/D-intrinsics.html
kind: editorial
---
# Comptime

Zith specifies `const` blocks, compile-time functions, reflection, and type
transformation. Zith-- does not evaluate arbitrary code at compile time yet.
Treat `const` as an immutability keyword, not as a request for compile-time
evaluation.

Layout intrinsics are the exception. `@` parses in expression position and the layout builtins work:

```zith
struct Point {
    x: i32,
    y: i32,
}

fn stride(): u64 {
    @sizeOf(Point)
}
```

`@sizeOf(T)` accepts any complete type and types as `u64`; `@sizeOf(void)` reports `E3001`. `@offsetOf(S, field)` and `@alignOf(S)` are struct-only and type as `i32`.

## What `const` means

This is a valid immutable binding:

```zith
const answer: i32 = 42;
```

It is not a `const fn` call and it does not cause arbitrary function execution
during compilation. A `const` initializer must use values accepted by the
current constant-expression rules. Function calls are not constant
expressions.

## What remains planned

Comptime evaluation, `const fn`, reflection helpers such as `@appendField`,
`@removeField`, and `@appendMethod`, and type transformation remain
specification-only. Read the [Comptime reference](doc:reference-11-comptime)
for the intended model and [Implementation Status](doc:reference-implementation-status)
for the current boundary.
