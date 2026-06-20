# Changelog

## [0.1.3] — 2026-06-20

### Added

- **AST Span fields** — every `ExprNode`, `StmtNode`, and `DeclNode` variant now carries a `memory::Span` for source location tracking. Added `exprSpan()`, `stmtSpan()`, `declSpan()` accessors to `AstBuilder`.
- **Parser span computation** — `spanFrom()` helper and span tracking throughout expression/statement/declaration parsing. All parse nodes now capture their source range.
- **ZIR bytecode IR** — `src/zir/zir/` with instruction model (26 opcodes), block/function/module types, bytecode emitter (HIR → ZIR), and stack-based interpreter. Not yet wired into the pipeline.
- **DynArray unit tests** — 14 test functions covering push, emplace, grow, reserve, move semantics, iteration, nontrivial destructors.
- **FlatMap unit tests** — 19 test functions covering insert, get, contains, erase, string keys, transparent lookup, rehash, iteration, move/copy semantics, 3724 assertions.
- **Version roadmap** — `.opencode/plans/version-roadmap.md` covering v0.2.0.0 through v0.8.0.0 with all 4 codegen paths.

### Changed

- **`std::vector` → `DynArray` (ZIR)** — `ZirBlock::code`, `ZirFn::blocks`, and `ZirModule::{functions, constants}` converted to arena-backed `DynArray`. `ZirBlock`, `ZirFn`, `ZirModule` now require an `Arena&` for construction.
- **`std::vector` → `DynArray` (Diagnostics)** — `Diagnostic::labels` and `Diagnostic::suggestions` converted to `DynArray`. `Diagnostic` now has a constructor taking `Arena&` plus the four core fields.
- **`ZirInterpreter` locals** — register file changed from `std::vector<Value>` to `std::array<Value, 16>` (always 16 slots).
- **`DynArray::emplace()`/`push()`/`reserve()`** — changed from brace `{}` initialization to parentheses `()` to avoid narrowing conversion errors with `uint32_t`/`uint8_t` parameters.
- **`TEST_MAIN` macro** — uses `#name` stringification instead of bare `name`.
- **`HeuristicEngine::generate()`** — takes `DynArray<std::string>&` instead of `std::vector<std::string>&`.

### New Tests

- 3 ZIR tests: `zithc-zir-inst`, `zithc-zir-emitter`, `zithc-zir-interp` (ported from prototype)
- `zithc-dyn-array-test`, `zithc-flat-map-test`

### Infrastructure

- **STATUS.md** updated — 27/27 tests passing, all subsystems documented
- **NEXT_STEPS.md** rewritten to match the version roadmap with phased checklists

## [0.1.2] — 2026-06-18

### Added

- **Name Resolution System** — new `Resolver` module (`src/import/resolver.*`) between import and sema stages. Resolves all identifiers to `SymId` via qualified/unqualified lookup with scope management and alias following.
- **RangeNode** — new AST node for `..` range syntax with lhs/rhs sub-expressions. Wired through builder and printer.
- **Parser extensions** — range operator `..`, arrow access `->`, fat arrow `=>`.
- **SymOrigin tracking** — `ImportManager::originOf()` records which file and local symbol each merged symbol originates from.
- **Error codes** — `NotNamespace` (2005) and `NoMember` (2006).
- **Resolver tests** — 10 test cases covering qualified, unqualified, scope, alias, and edge-case resolution.

### Changed

- **Pipeline** — added `Stage::Resolved` between `Imported` and `TypeChecked`. `CompilationSession` calls `resolveStage()` before sema.
- **SemaPipeline** — checks resolved side table before fallback `syms().lookup()` in `visitIdent`. Skips duplicate diagnostics when resolver already reported.
- **SymbolData** — added `SymId target` field for alias chain resolution. `declare()` / `declareInScope()` accept optional target parameter.
- **ImportManager** — import merge now tracks re-export duplicates properly, restoring declare_re_export duplicate-checking.
- **Diagnostics emit** — fixed `Optional(nullptr)` crash, bounds checks on `d.primary.end`, clamped unsigned subtraction.
- **Windows support** — `_WIN32` guards in `tool.cpp`, `run.cpp`, `project.cpp`, `interactive.cpp`, `deps.cpp`.

### Fixed

- Import merge re-exports no longer silently overwrite duplicates (now reports `DuplicateDecl`).

## [0.1.2] — 2026-06-13

### Added

- **TypeExpr AST** — dedicated type expression nodes (12 kinds) replacing ad-hoc type representation
- **TypeLower pass** — converts TypeExpr nodes into interned TypeIds
- **Solver stage** — generic constraint checking and monomorphization pass
- **Heuristic diagnostic engine** — "did you mean?" suggestions for undefined identifiers and type mismatches
- **Numeric bounds checking** — generic constraints for `T: i32`, `T: f32`, etc.
- **Platform detection** — `src/cli/platform.cpp` for host OS/arch identification
- **Resolver system** — name resolution pipeline for imports and symbols
- **Type walker** — utility for reducing type traversal boilerplate

### Changed

- **Pipeline stages** — Scanned, Imported, Resolved, TypeChecked, Solved, HirLowered, MirLowered, ZirInterpreted
- **Enum/Union/Component/Trait/Interface decls** — dedicated AST nodes instead of generic stubs
- **RangeNode** — new AST node for `..` range expressions
- **`->` and `=>` tokens** — added to lexer

### Fixed

- Critical bugs: TypeFn dangling span, empty-token-stream UB, lexer null fallthrough, SourceMap TOCTOU race, mod() depth parsing, back() const/Optional&lt;T*&gt; null guards
- Windows portability: `_WIN32` guards for `unistd.h`

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
