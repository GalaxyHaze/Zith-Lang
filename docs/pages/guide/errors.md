---
id: guide-errors
title: Errors
section: Language Guide
output: guide/D-errors.html
aliases: language/D-errors.html
kind: editorial
---
# Optional Values and Errors

Optional (`?T`) and failable (`T!`) types are working declared types. `null` coerces to `?T`, a plain `T` coerces to `?T`, and the postfix `?` operator propagates an optional operand inside a function that itself returns an optional.

```zith
fn halve(value: ?i32): ?i32 {
    let inner: i32 = value?;
    inner / 2
}
```

The recovery vocabulary around those types is not implemented. `fail`, `with`, `catch`, `must`, and `throw` remain specification-only. Keep examples to optionals and explicit branching until error values land.

`as` is a real cast that lowers to an LLVM conversion, covering numeric pairs and `raw opaque` <-> `*T`; other conversions are rejected and there is no narrowing overflow check. `is null` requires an optional operand, and `is Type` is implemented for tagged-union members and `opaque`. Flow-sensitive narrowing after an `is null` check on `?*T` does not exist yet, so `E3005` is reported where non-null pointer proof is required.

The planned rules are in the [Error Handling reference](doc:reference-08-error-handling); the current boundary is in [Implementation Status](doc:reference-implementation-status).

## Check before extracting

The clearest Zith-- pattern is an explicit branch:

```zith
fn portOrDefault(port: ?i32): i32 {
    if (port) {
        return port;
    }

    return 8080;
}
```

Use `raw value` only when another invariant proves that the optional contains
a value. It skips the normal check and is documented in [Raw & Unsafe](doc:guide-raw-unsafe).

## What is planned

The full Zith error model includes failable values, propagation, recovery, and
explicit failure. Zith-- currently accepts some of the syntax and types but
does not provide the complete `fail`/`with`/`catch`/`must`/`throw` workflow.
Do not write examples that depend on those forms unless you are reading the
full reference.
