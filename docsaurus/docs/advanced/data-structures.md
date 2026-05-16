---
id: data-structures
title: Data Structures
sidebar_label: Data Structures
description: Building custom data structures.
---

# Data Structures

Zith provides the tools to build efficient custom data structures with full ownership safety.

## Linked Lists

### Using Extension (No Weak Pointers)

```zith
struct Node<T> {
    value: T,
    next: share Node<T>?,
    prev: extension Node<T>?  // part-of relationship
}

struct LinkedList<T> {
    head: share Node<T>?,
    tail: extension Node<T>?,
    length: usize
}

impl<T> LinkedList<T> {
    fn new(): LinkedList<T> {
        LinkedList {
            head: null,
            tail: null,
            length: 0
        }
    }
    
    fn push(self: lend, value: T) {
        let new_node = share Node {
            value: value,
            next: null,
            prev: extension null
        };
        
        match self.tail {
            null -> {
                self.head = new_node;
            },
            _ -> {
                // Extension creates safe link
                self.tail.next = new_node;
            }
        }
        
        self.tail = new_node as extension;
        self.length += 1;
    }
}
```

## Trees

### Binary Search Tree

```zith
struct TreeNode<T> {
    value: T,
    left: share TreeNode<T>?,
    right: share TreeNode<T>?,
    parent: extension TreeNode<T>?
}

struct BST<T> {
    root: share TreeNode<T>?,
    size: usize
}
```

### Self-Referential Safety

The `extension` keyword prevents:
- Dangling pointers
- Use-after-free
- Need for weak pointers
- Unsafe code for cyclic structures

## Hash Tables

### Basic Implementation

```zith
struct HashMap<K, V> {
    buckets: Vec<Vec<(K, V)>>,
    capacity: usize,
    size: usize
}

impl<K, V> HashMap<K, V> where K: Hash + Eq {
    fn new(capacity: usize): HashMap<K, V> {
        let buckets = Vec::new();
        for _ in 0..capacity {
            buckets.push(Vec::new());
        }
        
        HashMap {
            buckets: buckets,
            capacity: capacity,
            size: 0
        }
    }
    
    fn insert(self: lend, key: K, value: V) {
        let idx = key.hash() % self.capacity;
        let bucket = self.buckets[idx];
        
        for (k, v) in bucket {
            if k == key {
                v = value;
                return;
            }
        }
        
        bucket.push((key, value));
        self.size += 1;
    }
    
    fn get(self: view, key: K): ?V {
        let idx = key.hash() % self.capacity;
        let bucket = self.buckets[idx];
        
        for (k, v) in bucket {
            if k == key {
                return v;
            }
        }
        null
    }
}
```

## Custom Allocators

### Arena Allocator

```zith
struct Arena {
    buffer: []u8,
    offset: usize
}

impl Arena {
    fn new(size: usize): Arena {
        Arena {
            buffer: unique [size]u8 { 0 },
            offset: 0
        }
    }
    
    fn alloc<T>(self: lend, value: T): *T {
        let size = @sizeOf(T);
        let align = @alignOf(T);
        
        // Align offset
        while self.offset % align != 0 {
            self.offset += 1;
        }
        
        let ptr = &self.buffer[self.offset] as *T;
        *ptr = value;
        
        self.offset += size;
        ptr
    }
}
```

## Performance Considerations

### Memory Layout

```zith
// Contiguous data for cache efficiency
struct FlatArray<T> {
    data: [N]T,
    length: usize
}

// Pointer-based for flexibility
struct LinkedNode<T> {
    value: T,
    next: share LinkedNode<T>?
}
```

### Zero-Cost Abstractions

- Generics compile to specific types
- No runtime overhead
- Inlined code

## Best Practices

- Use `extension` for hierarchical structures
- Prefer contiguous storage when possible
- Implement standard traits (Copy, Shared, etc.)
- Consider cache locality

## Standard Library

Zith's standard library provides:

- `Vec<T>` - Dynamic array
- `Map<K, V>` - Hash map
- `Set<T>` - Hash set
- `Deque<T>` - Double-ended queue

## See Also

- **[Memory Model](../language/memory.md)** - Ownership and NRM
- **[Ownership](../language/ownership.md)** - Extension keyword