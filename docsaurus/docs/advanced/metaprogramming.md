---
id: metaprogramming
title: Metaprogramming
sidebar_label: Metaprogramming
description: Code generation techniques and compile-time evaluation.
---

# Metaprogramming

Zith provides powerful compile-time metaprogramming capabilities through intrinsics and compile-time evaluation.

## Intrinsics (`@`)

Intrinsics are compile-time metadata and reflection accessed via `@` operator.

### Type Information

```zith
// Size of type
let size = @sizeOf(i32);           // 4
let arr_size = @sizeOf([10]i32);  // 40

// Alignment
let align = @alignOf(MyStruct);

// Members of struct
for member in @members(MyStruct) {
    println!("{}: {}", member.name, member.type);
}

// Check traits
if @hasTrait(MyType, Copy) {
    // Type can be copied
}
```

### Location Information

```zith
// Current file
let file = @file();

// Current line
let line = @line();

// Current function
let fn_name = @funcName();

// Create rich panic messages
panic!("Error at {}:{}", @file(), @line());
```

### Entity/Component Intrinsics

```zith
// Components of an entity type
for comp in @components(Player) {
    println!("{}", comp);  // Position, Health, ...
}

// Convert entity to struct
let struct_data = @toStruct(player);

// Convert struct back to entity
let entity = @toEntity(struct_data, scene);

// Register component dynamically
@registerComponent(entity, NewComponent { /* ... */ });
```

## Compile-Time Evaluation

### Const Expressions

```zith
const ARRAY_SIZE = @sizeOf([10]i32) / @sizeOf(i32);  // 10

const PAGE_SIZE = 4096;
const PAGES = HEAP_SIZE / PAGE_SIZE;

// Computed at compile time
const MAX_ITEMS = @sizeOf(Heap) / @sizeOf(Node);
```

### Must Be Compile-Time

```zith
// These must be computable at compile time:
const MEM_LAYOUT = @members(MyStruct);
const HAS_COPY = @hasTrait(MyType, Copy);
```

## Code Generation

### Procedural Macros

```zith
macro generate_accessors(field_name) {
    fn get_##field_name(self: view) -> FieldType {
        self.field_name
    }
    
    fn set_##field_name(self: lend, value: FieldType) {
        self.field_name = value;
    }
}

macro generate_impl(struct_name, fields...) {
    impl struct_name {
        // Generate for each field
        for field in fields {
            generate_accessors(field)
        }
    }
}
```

### Derive Macros

```zith
#[derive(Debug, Clone, Copy)]
struct Point {
    x: f32,
    y: f32
}

// Generates: Debug, Clone, Copy implementations
```

## Reflection

### Runtime Type Information

```zith
fn print_type<T>(value: T) {
    let type_info = @typeOf(value);
    println!("Type: {}", type_info.name);
    
    for member in @members(type_info) {
        println!("  {}: {}", member.name, member.type);
    }
}
```

### Dynamic Dispatch

```zith
let handler: any = create_handler();
@call(handler, "process", data);
```

## Build Scripts

```zith
// build.zith - runs before compilation

fn main() {
    // Generate code based on files
    let files = std.fs.readDir("src/");
    
    for file in files {
        if file.endsWith(".data") {
            generate_parser(file);
        }
    }
}
```

## Compile-Time vs Runtime

| Feature | Compile-Time | Runtime |
|---------|--------------|---------|
| `@sizeOf` | ✅ | ✅ |
| `@members` | ✅ | ✅ |
| `@typeOf` | ⚠️ | ✅ |
| Reflection | ❌ | ✅ |

## Best Practices

- Use compile-time for static computations
- Keep reflection minimal
- Generate code rather than runtime reflection
- Test generated code

## Use Cases

- Serialization/deserialization
- Protocol implementations
- Automated implementations
- Build-time code generation

## See Also

- **[Macros](./macros.md)** - Macro system
- **[Intrinsics](../language/intrinsics.md)** - Language intrinsics