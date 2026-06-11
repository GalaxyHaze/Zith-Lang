# Changelog

## [0.1.1] — 2026-06-07

### Added

- **`CompilationSession` pipeline integration** — redesigned as per-file driver with independent arenas, DI engine, AST builder, symbol table. Thread-safe by design (each file owns its state).
- **Diagnostic emitter** (`src/diagnostics/emit.cpp`) — source-aware output with snippet display, replaces old `emitter.cpp`.

### Changed

- **`cmd_check`** — now delegates to `CompilationSession` instead of inline lexer calls. Runs real lexer + parser (stub) pipeline per file.
- **`cmd_compile`, `cmd_build`, `cmd_run`, `cmd_execute`** — wired to `CompilationSession` with appropriate target stages.
- **`parseArgs`** — subcommand arg consumption limited to `New`, `Clean`, `Deps` only; `Build`, `Run`, `Check`, etc. no longer eat positional arguments as `subcommand_arg`.
- **`zithc-main.cpp`** — added missing `return` on `CompilerDriver::run()` so exit codes propagate correctly.
- **Diagnostic engine** — `emit()` and `emitTo()` moved from deleted `emitter.cpp` into `diagnostic-engine.cpp`.

### Fixed

- Subcommand positional args no longer stolen from `input_files` (e.g. `zithc build foo.zith` now correctly captures `foo.zith`).
- `CompilationSession::runTo()` returns `false` on stage hard-failure regardless of diagnostic count.
- `handleFile` aborts replaced with graceful `return false` for multi-file resilience.

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
