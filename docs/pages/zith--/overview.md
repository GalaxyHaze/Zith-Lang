---
id: zith-subset-overview
title: Zith-- Overview
section: Zith--
output: zith--/D-overview.html
aliases: language/D-zith-subset.html
kind: editorial
---
# Zith-- Overview

Zith-- is the language subset built by the `main` compiler and shipped through
`zithc`. Use this section when you want to write a program that the current
compiler can check, lower, and, when LLVM is available, build.

## What works today

The working subset includes:

- typed functions, structs, enums, unions, arrays, slices, pointers, and
  optionals;
- `let`, `var`, and `const` bindings;
- `if`, `for`, `when`, `defer`, and state-machine control flow;
- modules, aliases, visibility, generic functions and types;
- nominal traits, structural interfaces, and method dispatch through `dyn`;
- normal macros and `raw macro`;
- `extern fn`, C header imports, and `raw opaque` handles;
- `lend` and `view` parameters with call-site ownership annotations;
- HIR lowering, cache serialization, LLVM code generation, and WebAssembly
  execution.

The [Implementation Status](doc:reference-implementation-status) page is the
source of truth when a feature has a partial implementation or a known
restriction.

## What is different from full Zith

The larger Zith specification describes features that are not all available
in Zith--. Comptime evaluation, the complete NRA ownership proof, contexts,
words, tag macros, the full `unsafe` hierarchy, and the broader error model
remain outside the current subset.

The compiler reports unsupported syntax instead of silently accepting a
feature with different semantics. Read the [Zith reference](doc:zith-overview)
when you need the intended design, and return here before using it in a
program.

## A small program

```zith
struct Point {
    x: i32,
    y: i32,
}

fn distanceOnX(left: Point, right: Point): i32 {
    right.x - left.x
}

fn main(): i32 {
    let origin = Point { x: 0, y: 10 };
    let target = Point { x: 42, y: 10 };
    distanceOnX(origin, target)
}
```

Continue with the [Zith-- Language Guide](doc:guide-overview), or use the
[English Zith-- reference](doc:zith-subset-reference) for the subset contract.
