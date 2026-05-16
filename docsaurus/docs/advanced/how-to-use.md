---
id: how-to-use
title: How to Use Advanced Features
sidebar_label: How to Use
description: Guide to combining advanced features effectively.
---

# How to Use Advanced Features

This guide shows how to combine Zith's advanced features for powerful programming patterns.

## Quick Feature Reference

| Feature | Use For |
|---------|---------|
| `unique` | Exclusive ownership |
| `share` | Multiple owners |
| `view` | Read-only access |
| `lend` | Temporary mutation |
| `extension` | Hierarchical/part-of |
| `@` intrinsics | Compile-time reflection |
| Contexts | DSL creation |
| ECS | Data-oriented design |
| Markers | State machines |
| Flows | Reentrant logic |

## Combining Ownership with Generics

```zith
struct Container<T> {
    data: unique T
}

fn process<T: Copy + Shared>(container: Container<T>) {
    let copy = container.data;  // can copy
    let shared = container.data as share;  // can share
}
```

## Using Intrinsics with ECS

```zith
component Position { x: f32, y: f32 }
component Velocity { dx: f32, dy: f32 }

entity Player { Position, Velocity }

fn update_positions(scene: lend Scene) {
    // Get entity components at compile time
    for comp in @components(Player) {
        // Process based on component type
    }
    
    // Convert for threading
    for player in scene.entities<Player>() {
        let data = @toStruct(player);
        thread::spawn(|| process_entity(data));
    }
}
```

## Building a DSL with Contexts

```zith
// Define SQL DSL
context SQL {
    macro @select(cols...) { /* generate SELECT */ }
    macro @from(table) { /* generate FROM */ }
    macro @where(cond) { /* generate WHERE */ }
    
    tag macro @table(name) { name }
}

// Use the DSL
context SQL {
    let query = @select(name, age) 
                @from(@table(users))
                @where(age > 18);
}
```

## State Machines with Markers

```zith
fn game_loop() {
    marker init() {
        load_assets();
        if loaded {
            goto playing();
        } else {
            goto error();
        }
    }
    
    marker playing() {
        let event = poll_event();
        match event.type {
            QUIT -> goto shutdown(),
            UPDATE -> update_game(),
            RENDER -> render_frame()
        }
        goto playing();
    }
    
    marker shutdown() {
        save_state();
        cleanup();
    }
    
    marker error() {
        show_error();
    }
    
    entry { goto init(); }
}
```

## Custom Memory with Unsafe + Extension

```zith
struct Node<T> {
    data: T,
    next: share Node<T>?,
    prev: extension Node<T>?
}

arena Allocator {
    nodes: Vec<Node<T>>,
    
    fn alloc<T>(self: lend, value: T): extension Node<T> {
        let node = self.nodes.push(Node {
            data: value,
            next: null,
            prev: extension null
        });
        node as extension
    }
}
```

## Concurrency with Traits

```zith
trait Task {
    fn execute(self: view);
    fn priority(self: view): i32;
}

async fn run_tasks(tasks: Vec<impl Task>) {
    // Sort by priority
    tasks.sort_by(|a, b| a.priority() - b.priority());
    
    for task in tasks {
        await task.execute();
    }
}
```

## Error Handling + Contexts

```zith
context Network {
    macro @get(url) { /* fetch with error handling */ }
    macro @post(url, data) { /* post with retry */ }
}

context Network {
    fn fetch_data(): void! {
        let response = @get("https://api.example.com/data");
        process(response)!;
    }
    
    fn upload(data: []byte): void! {
        @post("https://api.example.com/upload", data)!;
    }
}
```

## Performance Patterns

### Zero-Cost Abstractions

```zith
// This compiles to efficient machine code
fn process<T: Drawable>(items: []T, canvas: lend Canvas) {
    for item in items {
        item.draw(canvas);  // inlined
    }
}
```

### Custom Allocators

```zith
scene GameWorld(require: 10000 entities) {
    policy: Grow;
    allocator: Linear;
}

// Entities use arena allocation
// Automatic cleanup when scene ends
```

## Migration Guide

### From Rust

- Replace `&mut` with `lend`
- Replace `Arc` with `share`
- Replace `Rc` with custom implementation
- Use `extension` instead of `RefCell`

### From C/C++

- Add ownership keywords
- Use `?T` instead of NULL checks
- Use `T!` for error handling
- Leverage ECS for game logic

### From Python/JS

- Start with `unique` by default
- Add `share` for shared state
- Use `view` for read-only functions
- Leverage traits for polymorphism

## Best Practices Summary

1. **Start Simple** - Use basic features first
2. **Add Complexity When Needed** - Don't over-engineer
3. **Prefer Safety** - Use ownership keywords
4. **Profile First** - Optimize when needed
5. **Document Complex Code** - Especially unsafe/advanced

## Common Patterns

- **Default to unique** - Most data exclusive
- **Use share for caches** - Multiple readers
- **Use view for APIs** - Clean interfaces
- **Use lend for methods** - Temporary mutation
- **Use extension for trees/graphs** - Hierarchical data

## See Also

- **[Memory Model](../language/memory.md)** - Ownership deep dive
- **[ECS](../language/ecs.md)** - Data-oriented design
- **[Contexts](../language/contexts.md)** - DSL creation