---
id: memory
title: Memory Model
sidebar_label: Memory
description: Understand Zith's Node Resource Model and memory safety guarantees.
---

# Memory Model

Zith's memory model is built on the **Node Resource Model (NRM)**, which provides compile-time memory safety without garbage collection.

## Core Concept: Nodes and Edges

All data in Zith is a **Node** in a **Directed Graph**:

- **Nodes:** Values (instances, data)
- **Edges:** Variables (references, bindings)
- **Roots:** Scope (functions, blocks, scenes)

```
    [Data Node]
         │
         ├── Edge 1 (primary reference)
         ├── Edge 2 (shared reference)
         └── Edge 3 (view reference)
```

## Memory Origins

NRM tracks where memory comes from:

### Stack

- Automatic allocation (scope-bound)
- Fast, efficient
- For small, short-lived data

```zith
fn process() {
    let local = 42;  // stack allocation
    // local is automatically freed when scope ends
}
```

### Heap

- Manual allocation
- Persistent, larger
- For data that must outlive its scope

```zith
let data = unique SomeLargeData.new();  // heap allocation
// Must be explicitly managed
```

### Arena/Allocator

- Custom allocation strategy
- Used via scenes for ECS
- Batch deallocation

```zith
scene GameWorld(require: 1000 entities) {
    policy: Grow;
}
```

## Validity Tracking

The compiler maintains two states for each node:

### Alive

Node exists and can be accessed:

```zith
let resource = unique Resource.new();
resource.use();  // ✅ valid
```

### Zombie

Node was moved/consumed; invalid in current branch:

```zith
let resource = unique Resource.new();
let other = resource;           // resource becomes zombie
other.use();                    // ✅ valid
resource.use();                 // ❌ ERROR: zombie
```

## Lazy Checking & Branch Analysis

If a node is used in one branch but not another:

```zith
let data = getData();

if condition {
    use(data);           // data is consumed
    return void;
} else {
    return data;         // data is still alive here
}

// Compiler infers: ?data (Optional based on path)
```

The compiler analyzes both branches and generates a union type.

## Ownership Keywords (Applied to Nodes)

The five keywords are rules applied on top of NRM:

```
NRM (Foundation)
    ↓
Keyword Rules (Applied)
    ├─ unique: only one edge alive
    ├─ share: multiple edges alive
    ├─ view: read-only access
    ├─ lend: exclusive mutable borrow
    └─ extension: part-of relationship
```

## Connectivity Analysis

The compiler performs connectivity analysis to verify memory safety:

```
Can I reach this Node from my current scope?
├─ Is there a valid path of edges?
├─ Is each edge valid (non-zombie)?
└─ Is the Node still allocated?
```

## Advantages Over Traditional Models

| Aspect | C | Rust | Zith |
|--------|---|------|------|
| Memory Safety | ❌ Manual | ✅ Lifetimes | ✅ NRM |
| Complexity | Low | High | Medium |
| Weak Pointers Needed | No | Yes | No |
| Unsafe Required | Yes | Sometimes | No |

## Extension: Eliminating Weak Pointers

The `extension` keyword enables self-referential structures without weak pointers:

```zith
struct Node<T> {
    value: T,
    next: share Node<T>?,
    prev: extension Node<T>?        // structurally part of parent
}

// No cyclic reference issues, no weak pointers needed
```

## Best Practices

- Default to `unique` for new data
- Use `share` only when multiple ownership is required
- Use `view` for read-only parameters
- Use `extension` for hierarchical data structures
- Prefer stack allocation when possible

## Next Steps

- **[Variables & Ownership](./variables.md)** - Ownership keywords
- **[Types](./types.md)** - Type system
- **[ECS](./ecs.md)** - Data-oriented architecture