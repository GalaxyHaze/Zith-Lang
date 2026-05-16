---
id: types
title: Type System
sidebar_label: Types
description: Explore Zith's static type system, primitive types, and type inference.
---

# Type System

Zith has a powerful static type system with features like optionals, failable types, generics, and traits.

## Primitive Types

```zith
// Integers
i8, i16, i32, i64, i128, isize
u8, u16, u32, u64, u128, usize

// Floating Point
f32, f64

// Boolean & Character
bool, char

// Unit (void)
void
```

## Type Modifiers

### Optional (`?T`)

Represents a value that may or may not exist.

```zith
let maybe: ?i32 = 42;
let nothing: ?i32 = null;

// Internal representation: union { T, null }
```

### Failable (`T!`)

Represents a computation that may fail.

```zith
fn divide(a: i32, b: i32): i32! {
    if b == 0
        throw DivisionByZero;
    a / b
}

// Internal representation: union { T, Error }
```

## Composite Types

### Structs

```zith
struct User {
    id: u64,
    name: []char,
    email: ?string,
    active: bool
}

let u = User {
    id: 123,
    name: "Alice",
    email: null,
    active: true
};
```

### Tuples (Product Types)

```zith
let pair: (i32, f32) = (42, 3.14);
let named: (x: i32, y: i32) = (x: 10, y: 20);

// Access
let a = pair.0;
let b = named.x;
```

### Arrays & Slices

```zith
let arr: [5]i32 = [1, 2, 3, 4, 5];      // fixed size
let slice: []i32 = arr[1..4];            // dynamic slice
```

### Enums (Nominal)

```zith
enum Color {
    Red,
    Green,
    Blue
}

enum Result<T, E> {
    Ok(T),
    Err(E)
}
```

### Union Types (Structural)

```zith
// Anonymous union
let value: union = switch expr {
    A -> (i32),
    B -> ([]char),
    C -> ()
};

// Named union
let ast: union(Ast) = switch parse() {
    Literal -> (i64),
    Operator -> (char)
};

// Union of types
let num: (i32 | f32) = 42;
```

## Generic Types

```zith
struct Box<T> {
    value: unique T
}

fn swap<T>(a: lend T, b: lend T) {
    let tmp = a;
    a = b;
    b = tmp;
}

// Monomorphization: static dispatch
// Dynamic dispatch: vtable
fn process<T: Drawable>(obj: Drawable<T>) { }
```

## Traits & Capabilities

Traits define capabilities that types can implement:

```zith
trait Copy {
    // Type is bitwise-copyable
}

trait Shared {
    // Safe to use in `share` contexts
}

trait Lent {
    // Safe to borrow mutably from global `unique`
}

implement Shared for MyType {
    // MyType can be shared across threads
}
```

## Type Navigation

### Optional Navigation (`?`)

```zith
let maybe: ?string = "hello";
let name = ?maybe or "default";      // "hello"

let nothing: ?string = null;
let name = ?nothing or "default";    // "default"
```

### Failable Navigation (`!`)

```zith
let content = !read_file();
let data = !parse_json(input) or JsonError { message: "invalid" };
```

### The `must` Operator

Require success or panic:

```zith
let name = must ?user.name;
// If null → panic!

let data = must !read_file();
// If fails → panic!
```

### The `raw` Operator

Bypass optional/failable checks:

```zith
let value = raw optional_value;  // access without navigation
```

## Type Inference

Zith infers types when possible:

```zith
let x = 10;              // infers i32
let name = "Alice";      // infers []char
let vec = Vec.new();     // infers Vec<T>
```

## Next Steps

- **[Variables & Ownership](./variables.md)** - Learn about ownership keywords
- **[Control Flow](./control-flow.md)** - Conditionals and loops
- **[Functions](./functions.md)** - Function declaration