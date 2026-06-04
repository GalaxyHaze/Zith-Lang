# Import System

Module loading, symbol resolution, and visibility management.

## Files

```
import/
├── import.h          # C API
├── import.hpp         # C++ wrappers
├── module-registry.hpp  # Module tracking
├── symbol-table.hpp   # Symbol scope tables
├── symbol-resolver.hpp  # Name resolution
└── import_spec.md    # Detailed specification
```

## Purpose

1. **Module Loading** — Load `.zith` files for imports
2. **Symbol Resolution** — Map identifiers to definitions
3. **Visibility** — Enforce public/private/protected

## Key Concepts

### Visibility Levels

| Level | Meaning |
|-------|---------|
| `public` | Exported, visible to importers |
| `protected` | Visible to derived/parents |
| `private` | Internal, not exported |

### Symbol Categories

- `types` — struct, union, component, entity, enum
- `functions` — Functions and methods
- `traits` — Traits and families

## C API

```c
// Load a module
ZithImport* zith_import_load(const char *path, ZithArena *arena);

// Resolve a symbol
ZithSymbol* zith_resolve(ZithImport *import, const char *name);

// Get public symbols
ZithSymbolArray zith_import_get_public_types(ZithImport *import);
```

## Symbol Table

The symbol table tracks definitions in scopes:

```cpp
class SymbolTable {
    void define(const char *name, ZithSymbol *sym);
    ZithSymbol* lookup(const char *name);
    void enter_scope();
    void exit_scope();
};
```

## Integration

- Parser uses import system during SCAN phase to collect symbols
- SEMA phase resolves identifiers via symbol table
- Codegen outputs imported symbols to LLVM IR

## See Also

- `import_spec.md` — Full specification with data structures
- `include/zith/zith.hpp` — Public types