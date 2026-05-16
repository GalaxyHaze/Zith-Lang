---
id: functions
title: Functions
sidebar_label: Functions
description: Learn about function declaration, parameters, return types, and advanced features.
---

# Functions

Zith supports multiple function types for different use cases.

## Standard Function

```zith
fn add(a: i32, b: i32): i32 {
    a + b
}

// Call
let result = add(5, 3);
```

## Async Function (Coroutine)

Async functions preserve stack state via `yield` and enable cooperative concurrency:

```zith
async fn fetch_data(url: []char): []char! {
    let response = !network.get(url);
    yield;  // give control to scheduler
    process(response)
}

// Usage
async fn main() {
    let data = await fetch_data("https://...");
}
```

## Markers (Structured Goto)

Markers are labeled blocks within a function for state machines and event loops:

```zith
fn event_loop() {
    marker wait() {
        let event = event_queue.wait();
        if event.type == QUIT
            goto exit;
        goto process(event);
    }

    marker process(event: Event) {
        handle(event);
        goto wait;
    }

    marker exit() {
        cleanup();
        return;
    }

    entry { goto wait; }
}
```

## Flows (Reentrant Goto)

Flows enable logic that can be re-entered from other contexts:

```zith
flow fn process_global() {
    marker handle(data) {
        // Can be jumped back to via anchor
        process(data);
    }

    entry { goto handle(initial_data); }
}
```

### Marker vs Flow

| Type | Use Case | Stack | Reentrant |
|------|----------|-------|-----------|
| `marker` | State machines, loops within function | Preserved | No |
| `flow fn` | Global reentrant logic | Preserved | Yes (via anchor) |

## Function Parameters

### By Value (Move)

```zith
fn process(data: MyData) {
    // data is moved into function
}
```

### By Reference

```zith
fn print(data: view MyData) {
    // read-only access
}

fn modify(data: lend MyData) {
    // mutable access, exclusive
}
```

### Pack Parameters

```zith
fn process(|a, b|) {
    // a, b are moved (unique by default)
}

fn with_ownership(|unique x, share y, view z|) {
    // explicit ownership in parameters
}
```

## Closures

```zith
let add = fn(a: i32, b: i32) -> i32 { a + b };

// With capture
let multiplier = 2;
let scaled = fn(x: i32) -> i32 { x * multiplier };
```

## Return Types

### Normal Return

```zith
fn get_value(): i32 {
    return 42;
}
```

### Implicit Return

```zith
fn get_value(): i32 {
    42  // last expression is return value
}
```

### Failable Return

```zith
fnRisky(): i32! {
    if condition
        throw Error("failed");
    42
}
```

## Function Overloading

Zith supports function overloading based on parameter types:

```zith
fn process(x: i32) { /* ... */ }
fn process(x: f32) { /* ... */ }
fn process(x: i32, y: i32) { /* ... */ }
```

## Best Practices

- Use markers for state machines and complex control flow
- Use `view` for read-only parameters
- Use `lend` for temporary mutation
- Prefer implicit returns for simple functions

## Next Steps

- **[Control Flow](./control-flow.md)** - Conditionals and loops
- **[Variables & Ownership](./variables.md)** - Ownership keywords
- **[Concurrency](./concurrency.md)** - Async and threading