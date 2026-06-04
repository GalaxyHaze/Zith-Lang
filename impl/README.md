# Zith Compiler Implementation

This directory contains the implementation of the Zith compiler, following a modular layer architecture.

## Pipeline Overview

```
Source Code (.zith)
       │
       ▼
┌──────────────────┐
│     Lexer         │  Tokens
│   (lexer/)       │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│     Parser        │  AST
│   (parser/)       │  ───► Three-pass pipeline
└────────┬─────────┘         (Scan → Expand → Sema)
         │
         ▼
┌──────────────────┐
│   Codegen         │  LLVM IR
│  (include/LLVM)  │
└────────┬─────────┘
         │
         ▼
    Executable
```

## Layers

| Layer | Directory | Purpose |
|-------|----------|---------|
| **Lexer** | `lexer/` | Convert source text to tokens |
| **Parser** | `parser/` | Parse tokens to AST (3-pass) |
| **AST** | `ast/` | AST node definitions |
| **Types** | `types/` | Type system definitions |
| **Import** | `import/` | Module system & symbol resolution |
| **Diagnostics** | `diagnostics/` | Error/warning reporting |
| **Memory** | `memory/` | Arena allocator |
| **CLI** | `cli/` | Command-line driver |

## Public C ABI

The public C API for embedding or extending Zith is in `include/zith/zith.hpp`. This header provides:

- Token definitions
- AST node structures
- Parser initialization and execution
- Diagnostic retrieval
- Source location tracking

## Code Generation

Code generation lives in `include/LLVM/` rather than in impl/. This separates the LLVM dependency from the core compiler and allows alternative backends (e.g., WASM, C transpiler) to be added later.

## Key Files

- `main.cpp` — Entry point, links all layers
- `parser/` — Contains detailed architecture docs in `parser.md`
- `import/import_spec.md` — Import system specification

## Building

```bash
mkdir build && cd build
cmake ..
make
```

## Testing

```bash
# Run all tests
./build/zith check tests/

# Run specific file
./build/zith run examples/01_basics.zith
```