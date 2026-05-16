---
id: macros
title: Macros
sidebar_label: Macros
description: Template and macro systems.
---

# Macros

Zith's macro system allows code generation and metaprogramming.

## Macro Types

### Declarative Macros

Pattern-based code generation:

```zith
macro assert(condition, message) {
    if !(condition) {
        panic!("Assertion failed: {}", message);
    }
}

// Usage
assert(x > 0, "x must be positive");
```

### Procedural Macros

Transform code at compile time:

```zith
macro derive(Debug) {
    // Generate Debug implementation
}

#[derive(Debug)]
struct Point {
    x: f32,
    y: f32
}
```

## Macro Syntax

### Simple Macros

```zith
macro log(message) {
    println!("[{}] {}", @file(), message);
}

// Expands to println with file info
log("Hello");  // [src/main.zith] Hello
```

### Macro with Parameters

```zith
macro add(a, b) {
    a + b
}

let result = add(1, 2);  // expands to 1 + 2
```

### Variadic Macros

```zith
macro print_all(args...) {
    for arg in args {
        print!("{} ", arg);
    }
    println!();
}

print_all(1, "hello", 3.14);
```

## Macro Hygiene

Zith macros maintain hygiene - identifiers don't accidentally capture:

```zith
macro swap(a, b) {
    let tmp = a;
    a = b;
    b = tmp;
}

fn example() {
    let x = 1;
    let y = 2;
    swap(x, y);
    // x and y are correctly swapped
}
```

## Tag Macros

Used in Contexts for DSL syntax:

```zith
context HTML {
    tag macro div(class: string, content) {
        "<div class=\"" + class + "\">" + content + "</div>"
    }
}

// Usage inside context
let page = <div class="container">Content</div>;
```

## Built-in Macros

```zith
@file()      // Current file
@line()      // Current line
@funcName()  // Current function
@sizeOf(T)   // Size of type
@members(T)  // Struct members
```

## Macro Rules

### Fragment Specifiers

```zith
macro rules {
    // Match expressions
    (expr) => { /* code */ }
    
    // Match statements
    (stmt) => { /* code */ }
    
    // Match types
    (ty) => { /* code */ }
}
```

### Recursion

```zith
macro for_each(item, list, body) {
    for item in list {
        body
    }
}

macro repeat(n, body) {
    if n > 0 {
        body
        repeat(n - 1, body)
    }
}
```

## Use Cases

### Reducing Boilerplate

```zith
macro impl_getters(struct_name, fields...) {
    // Generate getter methods
}

impl_getters(User, name, email, age)
```

### DSL Creation

```zith
context SQL {
    macro @select(columns...) {
        "SELECT " + columns.join(", ")
    }
    
    macro @from(table) {
        " FROM " + table
    }
}
```

## Debugging Macros

```zith
// View expanded macro
zith expand src/main.zith

// Verbose mode
zith build --expand-macros
```

## Best Practices

- Keep macros simple and focused
- Document complex macros
- Test macro expansion
- Consider if function would work

## See Also

- **[Metaprogramming](./metaprogramming.md)** - More advanced usage
- **[Contexts](../language/contexts.md)** - DSL namespaces