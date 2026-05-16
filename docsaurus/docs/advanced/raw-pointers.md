---
id: raw-pointers
title: Raw Pointers
sidebar_label: Raw Pointers
description: Complete raw pointer guide.
---

# Raw Pointers

Zith provides raw pointer types for low-level memory operations while maintaining type safety.

## Pointer Types

### Creating Pointers

```zith
// Create pointer from reference
let ptr = &value;           // *const T
let mut_ptr = &mut value;   // *mut T

// Create from address
let null_ptr: *i32 = null;
let from_addr = ptr addr 42; // *i32
```

### Dereferencing

```zith
let value = *ptr;           // read
*mut_ptr = 42;             // write

// Safe dereference with null check
if ptr != null {
    let val = *ptr;
}
```

## Pointer Arithmetic

```zith
let arr: [5]i32 = [1, 2, 3, 4, 5];
let first = &arr[0];
let second = first + 1;    // pointer offset
let fifth = first + 4;

// Calculate offset
let offset = (ptr as usize) - (base as usize) / size_of::<i32>();
```

## Working with Slices

```zith
fn process_slice(ptr: *i32, len: usize) {
    for i in 0..len {
        let value = *(ptr + i);
        process(value);
    }
}

// Create slice from pointer
let slice = ptr[..len];
```

## Interop with C

```zith
// Import C functions returning pointers
extern "C" {
    fn malloc(size: usize) -> *void;
    fn free(ptr: *void);
}

// Use C pointers
let mem = malloc(100);
free(mem);
```

## Safety Considerations

### Valid Pointers

```zith
// Ensure pointer is valid before dereferencing
fn safe_deref(ptr: *i32): i32! {
    if ptr == null {
        throw NullPointerError;
    }
    *ptr
}
```

### Lifetime Awareness

```zith
// Pointers don't track lifetime like references
// Must ensure data outlives pointer
fn dangerous() {
    let ptr: *i32;
    {
        let x = 42;
        ptr = &x;  // DANGER: x goes out of scope
    }
    // *ptr is undefined behavior!
}
```

## Converting Between Types

```zith
// Pointer type conversion
let int_ptr: *i32 = /* ... */;
let void_ptr = int_ptr as *void;

// Reinterpret bits
let bytes = ptr as *u8;
```

## Use Cases

- **FFI** - Interfacing with C libraries
- **Custom allocators** - Building specialized memory systems
- **Memory-mapped I/O** - Hardware registers
- **Embedded systems** - Direct hardware access

## Best Practices

- Prefer references when possible
- Use raw pointers only for FFI or low-level code
- Always validate null before dereferencing
- Ensure data lifetime exceeds pointer lifetime

## See Also

- **[Unsafe](./unsafe.md)** - Unsafe operations
- **[Memory Model](../language/memory.md)** - NRM and memory safety