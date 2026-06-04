# Type System

Type definitions, extended node IDs, and type qualifiers.

## Files

```
types/
└── types.hpp  # Centralized type definitions
```

## What's Here

### Extended Node IDs (1000+)

Base node kinds (0-999) are in `include/zith/zith.hpp`. Extended nodes (1000+) for advanced features:

- **Expressions**: Arrow calls, casts, lambdas, spawn, unwrap
- **Declarations**: Structs, enums, traits, components, entities, unions
- **Statements**: Switch, try-catch, spawn/await
- **Types**: Optional, result, arrays, pointers, packs

### Type Qualifiers

| Qualifier | Syntax | Meaning |
|-----------|-------|---------|
| optional | `T?` | Value may be null |
| failable | `T!` | Value may be error |
| shared | `~T` | Shared ownership |
| unique | `^T` | Unique ownership |
| view | `&T` | Borrowed reference |
| lend | `&mut T` | Mutable borrow |
| pack | `pack(T...)` | Parameter pack |

### Enums

```cpp
enum ZithFnKind {
    ZITH_FN_REGULAR,
    ZITH_FN_METHOD,
    ZITH_FN_CONSTRUCTOR,
    ZITH_FN_DESTRUCTOR,
    ZITH_FN_OPERATOR,
    ZITH_FN_LAMBDA,
};

 enum ZithOwnership {
    ZITH_OWN_MOVE,
    ZITH_OWN_COPY,
    ZITH_OWN_BORROW,
    ZITH_OWN_LEND,
 };

enum ZithVisibility {
    ZITH_VIS_PUBLIC,
    ZITH_VIS_PROTECTED,
    ZITH_VIS_PRIVATE,
};
```

## C++ Helpers

The header provides C++ type aliases for cleaner code:

```cpp
namespace zith {
    using TokenType = ZithTokenType;
    using SourceLoc = ZithSourceLoc;
    using Token = ZithToken;
}
```

## Integration

- Parser uses these definitions for node construction
- AST layer uses payload structs defined here
- Semantic analysis uses type qualifiers for checking

## Public C ABI

Base type enums are in `include/zith/zith.hpp` (C ABI contract). This header extends them for internal use.

## See Also

- `include/zith/zith.hpp` — Base types
- `ast/ast.h` — Payload structs
- `parser/parser_sema.cpp` — Type checking