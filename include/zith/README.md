# include/zith

Public headers of the Zith compiler/SDK.

## Public API Organization

- `zith.hpp`: aggregate facade for compatibility and single include point.
- `token.hpp`: tokens, source base types (`ZithSourceLoc`, `ZithStr`, token stream) and lexer.
- `ast.hpp`: AST nodes and node IDs.
- `parser.hpp`: parsing and parsing with import context.
- `memory.hpp`: public arena allocator.
- `import.hpp`: file/import utilities used by the public surface.
- `diagnostics.hpp`: public error codes.

## Public vs Internal Boundary

- Everything in `include/zith/*.hpp` (except `include/zith/impl/**`) is **public API**.
- Headers in `include/zith/impl/**` are **internal** and not part of a stable contract.
- External consumers should only include `zith.hpp` or granular public headers.

## Stability Conventions

### Stable (API/ABI)

- Exported C function signatures in these public headers.
- Layout of public structs (`ZithToken`, `ZithTokenStream`, `ZithNode`, etc.).
- Published numeric values of enums/tags that impact serialization, FFI or plugins.

Changes in these areas require explicit versioning and a migration note.

### Experimental

- Any symbol marked in the future with prefix/suffix `experimental` or feature macro.
- Content in `include/zith/impl/**`.
- C++ convenience helpers in the `ZITH` namespace may evolve faster, as long as they don't break the underlying C layer.

## Dependency Guideline

- Internal code may depend on the public API.
- Public API **must not** expose private types from `impl/`.
- Avoid public headers including headers from `impl/` directly or indirectly.
