# Memory Arena

Arena allocator for AST nodes and compiler data structures.

## Files

```
memory/
├── arena.hpp           # C++ wrapper
├── arena_c_functions.h  # C API
├── arena_c_functions.c # Implementation
└── utils.hpp          # ArenaList and utilities
```

## Purpose

Fast allocation of AST nodes without per-allocation overhead. Memory is allocated in large blocks (`arenas`), reducing `malloc` calls.

## Why Arena?

- **Speed**: Allocate in O(1) vs malloc O(log n)
- **Simplicity**: Single `arena_reset()` to free all
- **Cache**: Contiguous memory improves CPU cache

## C API

```c
// Create an arena (initial block size in bytes)
ZithArena* zith_arena_create(size_t initial_block_size);

// Allocate memory from arena
void* zith_arena_alloc(ZithArena *arena, size_t size);

// Reset arena (free all allocated memory)
void zith_arena_reset(ZithArena *arena);

// Get total used bytes
size_t zith_arena_used(const ZithArena *arena);

// Destroy arena
void zith_arena_destroy(ZithArena *arena);
```

## C++ Wrapper

```cpp
class ZithArena {
public:
    ZithArena(size_t initial_block_size = 4096);
    ~ZithArena();
    
    void* alloc(size_t size);
    void reset();
    size_t used() const;
    
    template<typename T>
    T* alloc() { return static_cast<T*>(alloc(sizeof(T))); }
};
```

## ArenaList

```cpp
// Dynamic array stored in arena
template<typename T>
using ArenaList = zith::ArenaList<T>;

// Usage:
ArenaList<ZithToken> tokens;
tokens.push_back(token, &arena);
```

## How It Works

1. Allocate large memory block (e.g., 4KB)
2. When `alloc()` called, bump pointer
3. If block full, allocate new block
4. On `reset()`, free all blocks at once

## Integration

- Parser uses arena for all AST node allocations
- Lexer uses arena for token storage
- `DiagList` uses arena for diagnostic messages

## See Also

- `include/zith/zith.hpp` — `ZithArena` definition