---
id: errors
title: Error Handling
sidebar_label: Errors
description: Learn about failable types, throw, try/catch, and structured error handling.
---

# Error Handling

Zith provides a rich error handling system based on failable types (`T!`) and structured resource management.

## Failable Types (`T!`)

Functions that may fail return `T!`:

```zith
fn divide(a: i32, b: i32): i32! {
    if b == 0
        throw DivisionByZero;
    a / b
}
```

## Throwing Errors

```zith
throw MyError("description");
throw MyError { field: value };
throw FileNotFound { path: filename };
```

## Try/Catch

### Basic Usage

```zith
try divide(10, 2) | result | {
    println!("success: {}", result);
} catch {
    DivisionByZero -> println!("divide by zero"),
    _ -> println!("other error")
}
```

### Pattern Matching in Catch

```zith
tryRiskyOperation() | value | {
    process(value);
} catch Error { code: 404, message: msg } -> {
    println!("Not found: {}", msg);
} catch Error { message: msg } -> {
    println!("Error: {}", msg);
} catch {
    _ -> println!("Unknown error");
}
```

## Navigation with Failable

### Basic Access

```zith
let content = !read_file();
```

### With Fallback

```zith
let data = !parse_json(input) or JsonError {
    message: "invalid JSON",
    input: input
};
```

### Chain Fallbacks

```zith
let data = !primary_source() or !backup_source() or throw NoData;
```

### Type Deduction

```zith
fn get_fallback(): i32 { 42 }  // never fails

let v1 = !op! or !op!;              // type: T!
let v2 = !op! or get_fallback();    // type: i32
```

## Do/Error/Drop

Structured resource management with three phases:

```zith
do ProcessFile {
    let file = !File.open("data.txt");
    let data = !file.read();
    process(data)!;
}
error ProcessFile {
    // Add context and re-throw
    throw FileProcessError {
        context: "failed to process file",
        cause: error
    };
}
drop ProcessFile {
    // Always executes (cleanup)
    file.close();
}
```

### Execution Order

1. `do` — acquire resources and work
2. `error` — intercept errors (if error occurred)
3. `drop` — cleanup (always)

## Error Propagation

Errors propagate automatically through failable functions:

```zith
fn process_file(path: []char): void! {
    let file = !File.open(path);
    let data = !file.read();
    let parsed = !parse_json(data);

    apply(parsed)
    // Errors propagate automatically
}
```

## The `must` Operator

Require success or panic:

```zith
let name = must ?user.name;
// If null → panic!("must required valid value at main.zith:42:?user.name")

let data = must !read_file();
// If fails → panic!("must required successful operation")

// In const (becomes static_assert)
const API_KEY = must !env("API_KEY");
// Compile-time failure if cannot determine
```

## The `raw` Operator

Bypass optional/failable checks when you're certain:

```zith
let value = raw optional_value;  // access without navigation
```

Use when you know the value is valid (external validation, performance-critical).

## Defining Custom Errors

```zith
enum MyError {
    Code(i32),
    Message(string),
    Complex { reason: string, context: string }
}

// Using
throw MyError.Code(42);
throw MyError.Message("something failed");
throw MyError.Complex { reason: "timeout", context: "network" };
```

## Best Practices

- Prefer `?...or` for optionals
- Prefer `!...or` for fallaibles
- Add context in error handlers
- Use do/error/drop for resource cleanup
- Avoid `must` except for initialization

## Next Steps

- **[Memory Model](./memory.md)** - Understanding validity
- **[Concurrency](./concurrency.md)** - Error handling in async
- **[Standard Library](./reference/stdlib.md)** - Built-in error types