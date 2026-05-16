---
id: syntax
title: Syntax Basics
sidebar_label: Syntax
description: Learn Zith's lexical structure, keywords, operators, and code conventions.
---

# Syntax Basics

Zith's syntax is designed to be explicit, readable, and consistent. This guide covers the fundamental building blocks of Zith code.

## Case Style

Zith uses **camelCase** as the default naming convention:

```zith
let userName = getUser();
fn processData() { }
var currentIndex = 0;
```

## Keywords

### Core Keywords

```
fn, let, var, const, type, struct, enum, union,
if, else, switch, for, while, return, break, continue,
do, error, drop, try, catch, marker, flow, entry, goto
```

### Ownership Keywords

```
unique, share, view, lend, extension
```

### Advanced Keywords

```
async, async fn, implement, require, trait, context, use, import,
comptime, static, global, scene, component, entity
```

## Operators

### Arithmetic

```zith
+, -, *, /, %, **
```

### Comparison

```zith
==, !=, <, >, <=, >=
```

### Logical

```zith
&&, ||, !
```

### Bitwise

```zith
&, |, ^, <<, >>
```

### Assignment

```zith
=, +=, -=, *=, /=, %=, &=, |=, ^=
```

### Navigation

```zith
?, !, must, raw
```

### Type System

```zith
as (type casting)
-> (pipeline)
., [] (access)
|...| (pack/tuple)
| (type union)
```

## Comments

```zith
// Line comment

/* Block comment
   spanning multiple lines */
```

## Identifiers

Identifiers start with a letter or underscore, followed by letters, digits, or underscores:

```zith
valid: foo, _private, varName2, __dunder__
invalid: 2fast, my-var, hello!
```

## Code Blocks

Blocks are defined with curly braces:

```zith
fn main() {
    let x = 10;
    if x > 5 {
        println!("x is large");
    }
}
```

## Semicolons

Semicolons are **optional** in Zith. The compiler infers statement boundaries from newlines and block structure:

```zith
let a = 1
let b = 2
let c = a + b  // no semicolon needed
```

## Next Steps

- **[Variables & Ownership](./variables.md)** - Learn about variable declaration
- **[Types](./types.md)** - Explore the type system
- **[Control Flow](./control-flow.md)** - Master conditionals and loops