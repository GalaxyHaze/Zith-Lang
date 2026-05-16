---
id: unsafe
title: Unsafe Operations
sidebar_label: Unsafe
description: Bypassing safety guarantees for performance-critical code.
---

# Unsafe Operations

Zith provides unsafe blocks for operations that cannot be verified at compile time. Use sparingly and with caution.

## When to Use Unsafe

- Interfacing with C libraries
- Performance-critical code
- Custom memory management
- Implementing low-level primitives

## Unsafe Blocks

```zith
unsafe {
    // Operations here bypass safety checks
    let ptr = raw_pointer;
    *ptr = 42;
}
```

## Unsafe Functions

```zith
unsafe fn get_unchecked<T>(slice: []T, index: usize): T {
    // No bounds checking
    let ptr = &slice[0];
    *(ptr + index)
}

// Call site
let value = unsafe get_unchecked(my_vec, 5);
```

## Dereferencing Raw Pointers

```zith
let ptr: *i32 = /* get from somewhere */;

unsafe {
    let value = *ptr;    // read
    *ptr = 42;          // write
}
```

## Type Casting

```zith
unsafe {
    // Reinterpret memory
    let bytes: *u8 = ptr as *u8;
    let other: *OtherType = bytes as *OtherType;
}
```

## Unchecked Indexing

```zith
let arr: [10]i32 = /* ... */;

unsafe {
    let first = arr[0 unchecked];
    let last = arr[9 unchecked];
}
```

## Calling Unsafe Functions

### External C Functions

```zith
extern "C" {
    unsafe fn malloc(size: usize) -> *void;
    unsafe fn free(ptr: *void);
}

// Usage
let mem = unsafe { malloc(100) };
unsafe { free(mem) };
```

### Rust-style Unsafe Functions

```zith
unsafe fn process_data(data: *i32, len: usize) {
    for i in 0..len {
        let val = *data;
        // process
        data = data + 1;
    }
}
```

## Unsafe Traits

```zith
unsafe trait Send {
    // Marker trait for thread transfer
}

unsafe impl Send for MyType {
    // Implementation guarantees safety
}
```

## Safety Requirements

When using unsafe, you must ensure:

1. **Memory Safety** - No use-after-free, no null dereference
2. **Type Safety** - Valid type conversions
3. **Alignment** - Proper pointer alignment
4. **No Data Races** - Synchronized access

## Documentation

Always document unsafe code:

```zith
/// Reads from hardware register at given address.
/// 
/// # Safety
/// - `addr` must be a valid hardware register address
/// - Register must be mapped and accessible
/// - No concurrent access to same register
unsafe fn read_register(addr: usize): u32 {
    /* ... */
}
```

## Common Patterns

### Implementing Unsafe Traits

```zith
unsafe implement Send for MyData {
    // Ensure MyData is actually safe to send
}

unsafe implement Sync for MyData {
    // Ensure MyData is safe to share across threads
}
```

### Manual Memory Management

```zith
unsafe {
    let ptr = allocate(size);
    initialize(ptr);
    // ...
    deallocate(ptr);
}
```

## Best Practices

- Minimize unsafe code
- Isolate unsafe in small functions
- Document safety requirements
- Test thoroughly

## See Also

- **[Raw Pointers](./raw-pointers.md)** - Pointer types
- **[Memory Model](../language/memory.md)** - Safety guarantees