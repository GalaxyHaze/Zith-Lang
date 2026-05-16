---
id: variables
title: Variables & Ownership
sidebar_label: Variables
description: Learn about variable declaration, mutability, and Zith's ownership keywords.
---

# Variables & Ownership

Zith provides explicit control over ownership through five keywords. Understanding these is essential for writing safe, efficient code.

## Variable Declaration

### Immutable (`let`)

```zith
let x = 10;              // immutable by default
let name = "Alice";
```

### Mutable (`var`)

```zith
var counter = 0;
counter += 1;            // allowed
```

### Constant (`const`)

```zith
const MAX_SIZE = 100;
const PI = 3.14159;
```

Constants are evaluated at compile-time and cannot be changed.

### Global Variables

```zith
// Shared across threads (requires Shared trait)
global score: share i32 = 0;

// Exclusive access (requires Lent trait)
global counter: unique i32 = 0;

// Access
global score += 10;
```

## Ownership Keywords

Zith has five ownership keywords that define how values are accessed and moved:

### `unique` — Exclusive Ownership

Only one reference can point to a node at any time. When assigned, the original becomes a zombie.

```zith
let resource = unique ResourceHandle.new();
let other = resource;              // resource is now zombie
other.use();                        // ✅ valid
resource.use();                    // ❌ ERROR: zombie
```

### `share` — Shared Ownership

Multiple references can point to the node simultaneously. All owners can mutate.

```zith
let data = Data.new();
let s1 = data as share;
let s2 = s1;                       // ✅ both valid
s1.mutate();                       // ✅
s2.read();                         // ✅
```

### `view` — Read-Only Borrow

Provides read-only access without ownership transfer. Cannot outlive the owner.

```zith
fn print_info(data: view Resource) {
    println!("{}", data.name);     // ✅ read
    data.modify();                 // ❌ ERROR: immutable
}

let res = Resource.new();
print_info(view res);              // ✅ res still valid
```

### `lend` — Mutable Borrow

Provides exclusive mutable access. The original cannot be accessed until the borrow returns.

```zith
fn increment(counter: lend i32) {
    counter += 1;
}

var x = 0;
increment(lend x);                 // ✅ x is borrowed
println!("{}", x);                 // prints 1, x is returned
```

### `extension` — Hierarchical Part-Of

Child is structurally part of parent; cannot outlive parent. Eliminates weak pointers and unsafe code.

```zith
struct Node<T> {
    value: T,
    next: share Node<T>?,
    prev: extension Node<T>?        // structurally part of parent
}
```

## Ownership Summary

| Keyword | Owns | Exclusive | Mutable | Thread Safe |
|---------|------|-----------|---------|-------------|
| `unique` | Yes | Yes | Yes | Yes (with Shared) |
| `share` | Yes | No | Yes | Yes (with Shared) |
| `view` | No | — | No | Yes |
| `lend` | No | Yes | Yes | Yes |
| `extension` | No | — | Yes | Yes |

## Best Practices

- **Default to `unique`** — Start with exclusive ownership
- **Use `share` intentionally** — Only when multiple owners are needed
- **Use `view` for reading** — For functions that only read data
- **Use `lend` for temporary mutation** — When you need to modify briefly
- **Use `extension` for self-referential structures** — Trees, linked lists, graphs

## Next Steps

- **[Types](./types.md)** - Explore the type system
- **[Memory Model](./memory.md)** - Deep dive into ownership
- **[Syntax](./syntax.md)** - Back to basics