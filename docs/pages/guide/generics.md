---
id: guide-generics
title: Generics
section: Language Guide
output: guide/D-generics.html
aliases: language/D-generics.html
kind: editorial
---
# Generics

Generic parameter lists use angle brackets and are accepted on `fn`, `struct`, `type` alias, `enum`, `union`, and `trait` declarations. Generic functions, structs, aliases, enums, unions, and `implement` blocks are monomorphized before HIR; explicit and inferred calls work.

```zith
struct Pair<T, U> {
    first: T,
    second: U,
}

fn identity<T>(value: T): T {
    value
}
```

Both declarations compile, and `identity<i32>(42)` plus inference `identity(42)` type-check and lower.

## Add a bound

Use `T: Trait` when the generic body needs a method or contract:

```zith
fn printAll<T: Printable>(value: T): T {
    value.print();
    value
}
```

Multiple bounds use `+`:

```zith
fn useBoth<T: Readable + Writable>(value: T) {
    value.read();
    value.write();
}
```

Interface bounds expose their declared fields and methods inside the generic
body. A call with an argument that does not satisfy a trait bound reports
`E3009`; an argument that does not satisfy an interface bound reports `E2024`.

Generic declarations are monomorphized before HIR. This means the compiler
checks each instantiated body and can lower it without a general-purpose
runtime generic representation.

The complete syntax is in the [Type System reference](doc:reference-03-type-system);
the boundary is in [Implementation Status](doc:reference-implementation-status).
