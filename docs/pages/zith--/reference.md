---
id: zith-subset-reference
title: Zith-- Reference
section: Zith--
output: zith--/D-reference.html
aliases: language/D-zith-subset-reference.html
kind: editorial
---
# Zith-- Reference

This is the English reference entry point for the language subset compiled by
the current `main` compiler. It describes the public surface that the
Zith-- guide teaches. The [Implementation Status](doc:reference-implementation-status)
page remains authoritative when a feature is partial.

## Declarations and functions

Functions use a colon before the return type:

```zith
fn add(left: i32, right: i32): i32 {
    left + right
}
```

Function declarations also accept the arrow spelling `-> ReturnType`, but the
colon form is the canonical spelling in this reference. Function types use
the colon form only: `fn(parameters): ReturnType`.

Function types are first-class values and lower to C-compatible function
pointers:

```zith
fn double(value: i32): i32 {
    value * 2
}

extern fn apply(operation: fn(i32): i32, value: i32): i32

fn main(): i32 {
    var operation: fn(i32): i32 = double;
    apply(operation, 21)
}
```

Function references, function-type parameters, and calls through those values
are implemented. Anonymous functions and closures are not implemented yet.
Use a named function and pass its function value instead.

## Packs and tuples

Zith calls its tuple-like aggregate a **pack**. Packs are heterogeneous
values with a fixed member order:

```zith
let response = | 200, "ok", true |;

let status: i32 = response[0];
let message: []char = response[1];
let accepted: bool = response[2];
```

Destructure a pack into local bindings with `[ ... ]`:

```zith
let [status, message, accepted] = response;
```

Pack indexes must be compile-time integer positions. Use an array or slice for
homogeneous data that needs runtime indexing. Named pack types can describe
the member names and types explicitly:

```zith
fn response(): |status: i32, message: []char, accepted: bool| {
    |status: 200, message: "ok", accepted: true|
}
```

Packs are implemented in Zith-- as concrete aggregates. The current subset
supports pack literals, named pack types, positional indexing, and binding
destructuring. Tuple methods, runtime pack indexing, and tuple-specific library
conventions are not part of the subset.

## Working language surface

Zith-- currently includes:

- `let`, `var`, and `const` bindings.
- structs, enums, unions, arrays, slices, packs, pointers, and `?T` optionals.
- `if`, `for`, `when`, `defer`, and `state`/`dock`/`jump`.
- modules, aliases, visibility, generic declarations, and overloads.
- traits, interfaces, `dyn` method dispatch, and `implement` blocks.
- normal macros and `raw macro`.
- `extern fn`, C header imports, and `raw opaque`.
- `lend` and `view` ownership annotations.

## Deliberately outside Zith--

The full Zith design also describes comptime evaluation, the complete NRA
ownership model, contexts, words, tag macros, anonymous functions, closures,
and the stronger `unsafe` hierarchy. Those topics belong to the [Zith
reference](doc:zith-overview) until the implementation status says otherwise.

For task-oriented explanations, continue with the [Zith-- Language
Guide](doc:guide-overview). For the formal source chapters, use the [full
Zith specification](doc:reference-specification).
