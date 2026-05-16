---
id: generics-deep
title: Generics Deep Dive
sidebar_label: Generics Deep Dive
description: Complex generic patterns.
---

# Generics Deep Dive

Advanced generic programming patterns in Zith.

## Higher-Kinded Types

### Concept

Higher-kinded types abstract over type constructors, not just types:

```zith
// Regular generic: takes a type
fn identity<T>(x: T): T { x }

// Higher-kinded: takes a type constructor
fn <F<*>> map_container<F>(container: F<i32>, f: fn(i32) -> i32): F<i32>
```

### Implementation

```zith
trait Functor<F<*>> {
    fn map<F<*>, A, B>(container: F<A>, f: fn(A) -> B): F<B>;
}

implement Functor for Vec {
    fn map<A, B>(vec: Vec<A>, f: fn(A) -> B): Vec<B> {
        let result = Vec::new();
        for item in vec {
            result.push(f(item));
        }
        result
    }
}
```

## Generic Constraints

### Advanced Constraints

```zith
// Multiple bounds
fn process<T: Copy + ToString + Eq>(item: T) { }

// Longer form
fn process<T>(item: T) where T: Copy + ToString { }
```

### Const Constraints

```zith
fn create_buffer<T>(size: usize) where @sizeOf(T) <= 256 {
    // Compile-time check
    let buffer: [size]T = /* ... */;
}
```

## Associated Types

### Advanced Usage

```zith
trait Graph {
    type Node;
    type Edge;
    
    fn nodes(self: view): Vec<Self::Node>;
    fn edges(self: view): Vec<Self::Edge>;
    fn connect(self: lend, from: Self::Node, to: Self::Node);
}

struct AdjacencyList {
    // Implementation details
}

impl Graph for AdjacencyList {
    type Node = usize;
    type Edge = (usize, usize);
    
    fn nodes(self: view): Vec<usize> {
        /* ... */
    }
    
    fn edges(self: view): Vec<(usize, usize)> {
        /* ... */
    }
    
    fn connect(self: lend, from: usize, to: usize) {
        /* ... */
    }
}
```

## Variadic Generics

### Packs

```zith
fn sum<T...>(args: |T...|): T where T: Add {
    var result = 0;
    for arg in args {
        result += arg;
    }
    result
}

sum(1, 2, 3, 4);  // T... = [i32, i32, i32, i32]
```

### Type-Level Computation

```zith
fn concat<T..., U...>(a: |T...|, b: |U...|): |T..., U...| {
    /* ... */
}
```

## GATs (Generic Associated Types)

```zith
trait Container {
    type Item<'a> where 'a: 'static;
    
    fn get<'a>(&'a self, index: usize) -> Self::Item<'a>;
}

struct Wrapper<T> {
    data: T
}

impl<T> Container for Wrapper<T> {
    type Item<'a> = &'a T;
    
    fn get<'a>(&'a self, index: usize) -> &'a T {
        &self.data
    }
}
```

## Trait Objects with Generics

### Erased Types

```zith
trait Handler {
    fn handle(self: view, event: Event);
}

// Concrete type
struct ClickHandler { }

implement Handler for ClickHandler {
    fn handle(self: view, event: Event) {
        /* ... */
    }
}

// Dynamic dispatch
let handler: Handler* = &ClickHandler { };
handler.handle(event);
```

## Monomorphization

### How It Works

```zith
fn process<T: Display>(item: T) {
    println!("{}", item.toString());
}

process(42);      // Generates process_i32
process("hello"); // Generates process_str
process(3.14);    // Generates process_f64
```

### Tradeoffs

| Aspect | Static Dispatch | Dynamic Dispatch |
|--------|------------------|-------------------|
| Performance | Zero overhead | Small vtable cost |
| Binary Size | Larger (per-type) | Smaller |
| Flexibility | Less (concrete types) | More (any impl) |

## Advanced Patterns

### Phantom Types

```zith
struct Vec<T, S> {
    data: [T],
    _phantom: S
}

struct Valued {}
struct Mutable {}

let a: Vec<i32, Valued> = /* ... */;
let b: Vec<i32, Mutable> = /* ... */;

// Different types, prevents mixing
```

### Newtype Pattern

```zith
struct UserId(u64);
struct OrderId(u64);

fn get_user(id: UserId) { }
fn get_order(id: OrderId) { }

let uid = UserId(42);
let oid = OrderId(42);

get_user(uid);  // OK
get_order(oid); // OK
// get_user(oid); // Error: wrong type!
```

## Best Practices

- Prefer static dispatch unless needed
- Use constraints to express intent
- Consider associated types for related types
- Use phantom types for type-level information

## See Also

- **[Generics](../language/generics.md)** - Basics
- **[Traits](./traits.md)** - Advanced traits