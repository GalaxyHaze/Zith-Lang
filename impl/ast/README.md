# AST Layer

Abstract Syntax Tree node definitions.

## Files

```
ast/
├── ast.h     # Node structs and payload definitions
└── ast.cpp   # Node constructors and utilities
```

## Node Structure

Zith uses a tagged union for AST nodes:

```cpp
typedef struct ZithNode {
    ZithNodeKind kind;      // Node type (enum)
    ZithSourceLoc loc;     // Source location
    union {
        ZithNode *kids[3];  // Child nodes (a, b, c)
        ZithNode **list;   // Array of nodes
        const char *ident; // Identifier name
        double num;        // Numeric literal
    } data;
} ZithNode;
```

## Node Categories

### Declarations
- `ZITH_NODE_FUNC_DECL` — Function definition
- `ZITH_NODE_STRUCT_DECL` — Struct definition
- `ZITH_NODE_VAR_DECL` — Variable declaration
- `ZITH_NODE_ENUM_DECL` — Enum definition
- `ZITH_NODE_IMPORT` — Import statement

### Statements
- `ZITH_NODE_BLOCK` — Block `{ ... }`
- `ZITH_NODE_IF` — Conditional
- `ZITH_NODE_FOR` — Loop
- `ZITH_NODE_RETURN` — Return statement
- `ZITH_NODE_EXPR_STMT` — Expression as statement

### Expressions
- `ZITH_NODE_BINARY_OP` — `a + b`
- `ZITH_NODE_UNARY_OP` — `-x`
- `ZITH_NODE_CALL` — Function call
- `ZITH_NODE_ACCESS` — Field access `obj.field`
- `ZITH_NODE_LITERAL` — Literals (int, float, string)

### Types
- `ZITH_NODE_TYPE` — Type reference
- `ZITH_NODE_POINTER_TYPE` — `*T`
- `ZITH_NODE_ARRAY_TYPE` — `[T]`

## Extended Payloads

Complex nodes use arena-allocated payload structs:

```cpp
typedef struct {
    const char *name;
    ZithNode **params;
    size_t param_count;
    ZithNode *return_type;
    ZithNode *body;
} ZithFuncPayload;
```

See `ast.h` for full list (VarPayload, ParamPayload, FieldPayload, etc.).

## Construction

```cpp
// Create a function node
ZithNode* zith_node_func_decl(
    ZithArena *arena,
    const char *name,
    ZithNode **params,
    size_t param_count,
    ZithNode *return_type,
    ZithNode *body
);

// Create a literal
ZithNode* zith_node_lit_int(ZithArena *arena, int64_t value);
ZithNode* zith_node_lit_float(ZithArena *arena, double value);
ZithNode* zith_node_lit_string(ZithArena *arena, const char *str);
```

## Integration

- Parser creates nodes via constructor functions
- SEMA phase annotates nodes with type information
- Codegen (`include/LLVM/`) walks the AST to generate IR

## See Also

- `include/zith/zith.hpp` — Base node kind enums
- `types/types.hpp` — Extended node IDs
- `parser/parser_decl.cpp` — Declaration parsing