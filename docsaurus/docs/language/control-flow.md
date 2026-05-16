---
id: control-flow
title: Control Flow
sidebar_label: Control Flow
description: Master conditionals, loops, pattern matching, and structured goto in Zith.
---

# Control Flow

Zith provides rich control flow constructs from traditional conditionals to structured goto via markers.

## Conditionals

### If Expression

```zith
if condition {
    // code
}

if x > 10 {
    println!("large");
} else {
    println!("small");
}
```

### If as Expression

```zith
let result = if x > 10 { "large" } else { "small" };
```

### Switch Expression

```zith
let value = switch x {
    1 -> "one",
    2 -> "two",
    _ -> "other"
};
```

### Pattern Matching with Switch

```zith
match optional_value {
    Some(x) -> println!("got {}", x),
    None -> println!("nothing")
}

// With guards
match item {
    User { age: a } if a >= 18 -> "adult",
    User { age: _ } -> "minor",
    _ -> "unknown"
}
```

## Loops

### For Loop

```zith
for i in 0..10 {
    println!("{}", i);
}

// Iterate over collection
for item in collection {
    process(item);
}
```

### While Loop

```zith
var i = 0;
while i < 10 {
    println!("{}", i);
    i += 1;
}
```

### Infinite Loop with Break

```zith
loop {
    let event = poll();
    if event.type == QUIT {
        break;
    }
    process(event);
}
```

### Loop with Condition

```zith
do {
    let data = !fetch();
    if data.ready {
        break;
    }
} while true;
```

## Early Exit

### Return

```zith
fn find_item(items: []Item, target: u32): ?Item {
    for item in items {
        if item.id == target {
            return item;  // early return
        }
    }
    null  // not found
}
```

### Break

```zith
for i in 0..100 {
    if complex_condition(i) {
        break;  // exit loop
    }
}
```

### Continue

```zith
for item in items {
    if !item.valid {
        continue;  // skip to next
    }
    process(item);
}
```

## Goto with Markers

Markers provide structured goto for complex state machines:

```zith
fn parse_json() {
    marker parse_value() {
        let ch = peek();
        if ch == '{' {
            goto parse_object();
        }
        if ch == '[' {
            goto parse_array();
        }
        goto parse_literal();
    }

    marker parse_object() {
        consume('{');
        if peek() == '}' {
            consume('}');
            goto done();
        }
        goto parse_value();  // parse first member
    }

    marker done() {
        return;
    }

    entry { goto parse_value(); }
}
```

### Goto Rules

- Markers can access parent scope variables (view or lend)
- Cannot move local variables (static analysis)
- Cannot jump to a Scene (unidirectional)

## Error Handling

See [Error Handling](./errors.md) for try/catch, throw, and do/error/drop.

## Pipeline Operator

The `->` operator enables function chaining:

```zith
let result = data
    -> transform()
    -> filter()
    -> collect();
```

## Best Practices

- Use switch/match for complex conditionals
- Use markers for state machines (not raw goto)
- Prefer early returns for simple cases
- Use do/error/drop for resource management

## Next Steps

- **[Errors](./errors.md)** - Error handling patterns
- **[Functions](./functions.md)** - Marker and flow functions
- **[Syntax](./syntax.md)** - Language basics