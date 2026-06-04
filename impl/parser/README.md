# Parser Layer

Parses token stream into an Abstract Syntax Tree (AST).

## Files

```
parser/
├── parser.cpp         # Entry point, 3-pass pipeline
├── parser_utils.cpp  # Token navigation, error recovery
├── parser_expr.cpp   # Expression parsing (Pratt parser)
├── parser_decl.cpp   # Declarations & statements
├── parser_sema.cpp   # Semantic analysis
├── parser_test.cpp   # Test utilities
└── parser.md        # Detailed architecture docs
```

## Three-Pass Pipeline

```
         Tokens
            │
    ┌───────┴───────┐
    │   SCAN PHASE  │  ← Collect signatures (fn, struct, import)
    │               │     Bodies stored as UNBODY (raw tokens)
    └───────┬───────┘
            │
    ┌───────┴───────┐
    │  EXPAND PHASE │  ← Parse function bodies
    │               │     UNBODY → BLOCK
    └───────┬───────┘
            │
    ┌───────┴───────┐
    │  SEMA PHASE  │  ← Semantic analysis
    │               │     Type checking, name resolution
    └───────┬───────┘
            │
           AST
```

## Why Three Passes?

1. **SCAN**: Must know all function signatures BEFORE parsing bodies (for mutual recursion)
2. **EXPAND**: Function bodies can only be parsed after signatures are known
3. **SEMA**: Type checking requires fully parsed AST

## Key Functions

```cpp
// Main entry point
ZithAstNode* zith_parse_with_source(const char *source, ZithArena *arena);

// Phase-specific
void run_parser_phase(Parser *p, ZithParserMode mode);
void expand_unbody(Parser *p, ZithNode *node);
void sema_run(Parser *p, ZithNode *root);
```

## Pratt Parser

Expression parsing uses [Pratt parsing](https://en.wikipedia.org/wiki/Pratt_parser) (top-down operator precedence) for correct handling of:

- `a + b * c` → `a + (b * c)` (precedence)
- `a == b == c` → (a == b) == c (left-associative)
- `-x + y` → (-x) + y (unary prefix)

## Semantic Analysis (SEMA)

The SEMA phase validates:
- Name resolution (undefined identifiers)
- Type compatibility
- Return statement validation
- Scope management
- Binary operation types

## See Also

- `parser.md` — Detailed architecture and data flow
- `parser_sema.cpp` — Semantic analysis implementation
- `ast/ast.h` — Node definitions