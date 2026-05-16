---
id: generics
title: Generics
sidebar_label: Generics
description: Learn about generic types, traits, and type parameters in Zith.
---

# Generics

Zith supports powerful generic programming with compile-time type parameters and trait constraints.

## Generic Types

### Generic Structs

```zith
struct Box<T> {
    value: unique T
}

let int_box = Box<i32> { value: 42 };
let str_box = Box<string> { value: "hello" };
```

### Generic Functions

```zith
fn swap<T>(a: lend T, b: lend T) {
    let tmp = a;
    a = b;
    b = tmp;
}

fn first<T>(items: []T): ?T {
    if items.length > 0 {
        items[0]
    } else {
        null
    }
}
```

### Generic Enums

```zith
enum Result<T, E> {
    Ok(T),
    Err(E)
}

enum Optional<T> {
    Some(T),
    None
}
```

## Traits & Capabilities

Traits define capabilities that types can implement:

### Defining Traits

```zith
trait Copy {
    // Type is bitwise-copyable
}

trait Shared {
    // Safe to use in `share` contexts (thread-safe)
}

trait Lent {
    // Safe to borrow mutably from global `unique`
}

trait ToString {
    fn toString(self): string;
}

trait Drawable {
    fn draw(self: view, canvas: lend Canvas);
}
```

### Implementing Traits

```zith
struct Point {
    x: f32,
    y: f32
}

implement ToString for Point {
    fn toString(self): string {
        "({}, {})".format(self.x, self.y)
    }
}

implement Drawable for Point {
    fn draw(self: view, canvas: lend Canvas) {
        canvas.drawPoint(self.x, self.y);
    }
}
```

## Trait Constraints

### Single Constraint

```zith
fn process<T: Copy>(item: T): T {
    item  // can copy
}
```

### Multiple Constraints

```zith
fn save<T: Shared + ToString>(item: T) {
    let data = item.toString();
    // save data
}
```

### Where Clauses

```zith
fn process<T>(items: []T) where T: Copy + ToString {
    for item in items {
        println!("{}", item.toString());
    }
}
```

## Static vs Dynamic Dispatch

### Static Dispatch (Monomorphization)

```zith
fn process<T: Drawable>(obj: Drawable<T>) { }
// Compiles to specific types, no runtime overhead
```

### Dynamic Dispatch (vtable)

```zith
fn process_dyn(obj: Drawable*) {
    obj.draw();  // uses vtable at runtime
}
```

## Associated Types

Traits can have associated types:

```zith
trait Container {
    type Item;

    fn get(self: view, index: usize): ?Item;
    fn add(self: lend item: Item);
}

struct Vec<T> { ... }

implement Container for Vec<T> {
    type Item = T;

    fn get(self: view, index: usize): ?T {
        if index < self.length {
            self.data[index]
        } else {
            null
        }
    }

    fn add(self: lend item: T) {
        self.push(item);
    }
}
```

## Generic Implementations

```zith
struct Pair<T, U> {
    first: T,
    second: U
}

implement<T, U> ToString for Pair<T, U> where T: ToString, U: ToString {
    fn toString(self): string {
        "({}, {})".format(self.first.toString(), self.second.toString())
    }
}
```

## Variance

Zith infers variance automatically:

- **Covariant:** `Box<Sub>` can be used as `Box<Base>`
- **Contravariant:** `fn(Super)` can be used as `fn(Sub)`

## Best Practices

- Use generics to reduce code duplication
- Constrain generics with traits for flexibility
- Prefer static dispatch (monomorphization) for performance
- Use dynamic dispatch when type erasure is needed

## Next Steps

- **[Types](./types.md)** - Type system basics
- **[Traits (Advanced)](../advanced/traits.md)** - Deep dive into traits
- **[Modules](./modules.md)** - Code organization