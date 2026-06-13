# Architecture Comparison: legacy → main

## Top-Level Layout

| legacy | main | Notes |
|--------|---------|-------|
| `impl/` | `src/` | Flat impl/ → modular src/ with subsystem dirs |
| `include/zith/` | *(removed)* | Public C/C++ headers removed; all headers internal under src/ |
| `lib/` | `stdlib/` | Standard library renamed, restructured |
| `lsp/` | *(removed)* | LSP server pulled out; deferred |
| `docsaurus/` | *(removed)* | Docs site builder removed |

## Subsystem Breakdown

### CLI (`impl/cli/` → `src/cli/`)

**legacy:** CLI11-based dispatch, manual subcommand dispatch in `CLI.cpp`, flat command files in `cmd/`.

**main:** Clean `options.hpp` parsing, `CompilationSession` per-file pipeline driver, `PipelinePlan` enum-based stage tracking, proper project config. Commands organized as standalone files in `src/cli/cmd/`.

### Parser (`impl/parser/` → `src/parser/`)

**legacy:** Monolithic `parser_decl.cpp` (~1k LOC), `parser_expr.cpp`, `parser_sema.cpp` (~1.4k LOC), `parser_utils.cpp`, separate `type_checker.cpp`. Extensive test suite in `tests/test_parser.cpp` (~1.3k LOC).

**main:** Clean class hierarchy with Pratt-style expression parsing. All methods currently return stubs (`kInvalid*` / `false`). Scoped to `src/parser/parser.cpp`, `parser.hpp`, `recovery.hpp`.

### Memory (`impl/memory/` → `src/memory/`)

**legacy:** Arena, utils, file.c, arena_c_functions.c. Mix of C and C++.

**main:** Arena allocator, dyn-array, optional<T>, result<T,E>, span, string-interner, source-file, source-map. Pure C++, production-quality, foundation of all subsystems. C interop removed.

### Diagnostics (`impl/diagnostics/` → `src/diagnostics/`)

**legacy:** Diagnostic builder, bag, heuristic engine, emitter, span, format, lazy_message.

**main:** Clean `DiagnosticEngine`, `emit.cpp` with ANSI color support, `error-codes.hpp` enum. No heuristic engine. Source-aware snippets not yet implemented.

### Import (`impl/import/` → `src/import/`)

**legacy:** Import system, module registry, symbol resolver, symbol table.

**main:** Import manager, symbol table with scope management, resolver, symbol-id, symbol-visibility. More modular, properly separated concerns.

### Type System (`impl/types/` → `src/types/`)

**legacy:** types.hpp (C-style type definitions), typesystem.hpp.

**main:** TypeIntern for deduplication, Unifier for type unification, TypeKind enum with full variant set. Currently stubs — all return hardcoded constants.

### LLVM Backend

**legacy:** `impl/llvm/backend.cpp`, `impl/llvm/frontend.cpp`, `include/LLVM/`.

**main:** Removed entirely. LLVM backend deferred to Phase 6.

### VM / Interpreter

**legacy:** `impl/vm/decoder.cpp`, `impl/vm/encoder.cpp`, `include/zith/vm.hpp`, `include/zith/zbcBuilder.h`.

**main:** Replaced with ZIR intermediate representation split into HIR (high-level IR) and MIR (mid-level IR). Interpreter not yet started (Phase 6).

### AST (`impl/ast/` → `src/ast/`)

**legacy:** Monolithic `ast.cpp` (~1.4k LOC), C-style `include/zith/ast.h`.

**main:** Arena-backed AST builder with ID-based handles, `ast-nodes.hpp`, `ast-ids.hpp`, `ast-printer`. Cleaner design decoupled from C header constraints.

### New: Sema (`src/sema/`)

**main only:** `SemaPipeline` orchestrates type-checking, name resolution, and HIR lowering in one pass. `SemaContext` wires symbol table, types, diagnostics. Not present in legacy.

### New: ZIR (`src/zir/hir/`, `src/zir/mir/`)

**main only:** Two-tier IR:
- **HIR:** High-level IR with HirExpr (literals, binary, unary, let, var, call, ret, branch, jump, phi). Close to AST but typed.
- **MIR:** Mid-level IR with 22 opcodes, operands, basic blocks. Lowered from HIR.

## Key Architectural Shifts

1. **Thread-safe per-file compilation** — `CompilationSession` owns all its state; files don't share mutable state. Enables future parallel compilation.
2. **Arena-allocation everywhere** — All AST, types, symbols, and IR nodes allocated from arenas. No global thread_local state.
3. **C++20** (was C++23) — Broadens compiler support.
4. **FetchContent dependencies** — mio (memory-mapped I/O), tomlplusplus (TOML parsing). No CLI11 (removed).
5. **Explicit pipeline stages** — `Stage` enum defines clear compilation phases: Source → Lexed → Parsed → Resolved → TypeChecked → HirLowered → MirLowered → ZirInterpreted.
6. **No public C API** — All headers internal.
7. **Separated concerns** — Sema pipeline, HIR/MIR split, import system modularized.
