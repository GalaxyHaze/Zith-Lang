# Changelog

## [0.1.0] — 2026-05-30

### Added (Rewrite Branch)

- **Lexer** — full tokenizer with number/string/identifier/comment handling, perfect-hash keyword lookup
- **Source layer** — `SourceLoc` (mmap-backed file reading), `SourceMap` (thread-safe registry), `Span`/`Loc` types
- **Infrastructure** — `Result<T,E>`, `Optional<T>`, `Arena` allocator, `DynArray<T>`, `StringInterner`

### Stubs Created

- Diagnostics model + engine + emitter
- AST node types + builder
- Parser skeleton + recovery
- Symbol table + resolver
- Type system (intern, unify)
- Sema pipeline + context
- HIR/MIR modules and lowering passes
- Backend interface + VM bytecode format
- Driver + compilation session
- Test targets: lexer, parser, sema, mir

### Fixed

- Lexer `tokenize()` now returns a populated `TokenStream` instead of empty struct
- Static lexer globals refactored into `Lexer` class (re-entrant)
- All filenames normalized to snake-case-with-hyphens
- Removed stale underscore-named placeholder files

### Infrastructure

- CMake test targets for all 5 test suites
- C++23 standard enforced
- `mio` dependency for memory-mapped I/O
