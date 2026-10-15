# Contributing to Zith

## Building

```bash
cmake -B build
cmake --build build
```

Requires C++23, CMake 3.20+, and LLVM (for codegen).

## Project structure

```
src/
  memory/       - Arena allocator, DynArray, FlatMap, and interning
  support/      - Shared platform and compiler utilities
  common/       - Cross-subsystem utilities and common types
  cli/          - CLI argument parsing and command dispatch
  session/      - Compilation pipeline orchestration
  diagnostics/  - Error codes, diagnostics, and reporting
  cache/        - Incremental compilation and artifact storage
  capi/         - C ABI wrapper
  codegen/      - LLVM IR code generation
  comptime/     - Compile-time evaluation and generic instantiation
  formatter/    - Source code formatter
  hir/          - High-level intermediate representation
  sema/         - Semantic analysis, NRA preparation, and HIR lowering
  symbols/      - Symbol tables, imports, and name resolution
  types/        - Type interning, unification, and lowering
  wasm/         - WebAssembly playground and WASI support
  zirl/         - Zith IR library format
  frontend/     - Lexing, parsing, AST lowering, macros, and frontend printing
  cinterop/     - C header bindings
  cc/           - Companion C compilation
  ide/          - Editor and language-server integration
  interp/       - HIR interpreter
  vm/           - Portable VM v2
```

## Code style

- clang-format (see `.clang-format` at root)
- Namespaces match directory names under `src/`
- Arena-backed allocation throughout
- No exceptions, no RTTI

## Pipeline

`CompilationSession` in `src/session/` coordinates compilation. For the
current stage names and supported paths, use
[docs/impl-status.md](docs/impl-status.md) as the source of truth.

## Implementation Status

See [docs/impl-status.md](docs/impl-status.md) for the current state of every language feature and CLI command.
