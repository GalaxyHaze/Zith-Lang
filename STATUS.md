# Zith Compiler — Implementation Status

**Branch:** main  
**Date:** 2026-06-11  
**Build:** `cmake -B build && cmake --build build` — clean compile  
**Tests:** `ctest --test-dir build` — **7/7 passing**

---

## Legend

| Icon | Meaning |
|------|---------|
| \* | Fully implemented |
| \*\* | Partial (works but has gaps) |
| \*\*\* | Stub / not yet implemented |
| \*\*\*\* | Missing / broken |

---

## Pipeline Overview

```
Source → Lexer → Parser → AST → Sema → HIR → MIR → ZIR → (Interpreter)
          *       (*)     *     ***    **     ***    ****      ****
```

(*) Parser skeleton wired into pipeline; actual parsing returns stubs.

---

## Subsystem Status

### Memory (`src/memory/`) \*
Arena allocator, dyn-array, optional, result, string interner.  
All production-quality. The foundation of the entire compiler.

### Lexer (`src/lexer/`) \*
Full tokenizer with:
- Number literals (decimal, hex `0x`, octal `0c`, binary `0b`, float)
- String/char literals with escape sequences
- Comments (`//`, `/* */`, `///`, `/** */`)
- Keyword lookup via compile-time perfect hash (130+ keywords)
- Operator/punctuation tokenization

### Source Management (`src/parser/`) \*
- `Span`, `Loc` types
- `SourceFile` — mmap-based and string-backed file loading
- `SourceMap` — thread-safe registry with caching
- `Recovery` — basic panic-mode error recovery

### AST (`src/ast/`) \*
- Full AST data model (8 expression types, 3 statement types, 3 declaration types)
- Arena-backed builder with ID-based handles
- Binary/unary operator enums

### Diagnostics (`src/diagnostics/`) \*\*
- Diagnostic engine, severity levels, labels — \*
- Emitter — functional but **no source-aware output** (no snippets, underlines)

### Import System (`src/import/`) \*\*
- Symbol table with scope management — \*
- `declare()`, `lookup()`, `lookupInScope()` — \*
- Visibility (`pub`, `priv`, `mod(n)`) — \*
- Import resolution (`import`, `from`, `export`) — \*
- Name resolver — \*

### CLI (`src/cli/`) \*\*

| Component | Status | Notes |
|-----------|--------|-------|
| Argument parsing (`parseArgs`) | \* | 21 flags, 13 subcommands, full validation |
| `ZithFlags.toml` loader | \* | Reads default config from compiler-relative path |
| Config merging | \* | CLI flags → ZithFlags.toml cascade |
| `cmd_help`, `cmd_version` | \* | Ported from legacy |
| `cmd_new` | \* | Full project scaffolding |
| `cmd_check` | \*\* | Uses `CompilationSession` pipeline, real lexer, reports pass/fail |
| `cmd_compile` | \*\* | Uses `CompilationSession`, runs through MIR lowering |
| `cmd_build` | \*\* | Uses `CompilationSession`, full pipeline |
| `cmd_run` | \*\* | Compile + execute via pipeline |
| `cmd_execute` | \*\* | Full pipeline + execution stub |
| `cmd_fmt` | \*\*\* | Validates parse, no re-indent yet |
| `cmd_test` | \*\*\* | Not implemented |
| `cmd_docs` | \*\*\* | Not implemented |
| `cmd_repl` | \*\*\* | Not implemented |
| `cmd_clean` | \*\* | Recognizes target, no-op |
| Pipeline stages (`CompilationSession`) | \*\* | Per-file pipeline driver; `lexStage` wired to real lexer, `parseStage` to parser (stub), rest TODO |

### ZithProject.toml integration \*\*\*
Not yet wired in. `main()` has a TODO placeholder.

### Parser (`src/parser/`) \*\*\*
- Class hierarchy designed with Pratt-style expression parsing — \*\*
- **Actual parsing:** all methods return `kInvalid*` / `false` — \*\*\*

### Sema (`src/sema/`) \*\*\*
- Context object (wiring for symbol table, types, diagnostics) — \*
- `SemaPipeline::run()` — **discards the program and returns empty result** — \*\*\*

### Type System (`src/types/`) \*\*\*
- Data model (type kinds, width enums, TypeData variant) — \*
- `TypeIntern` — all methods return hardcoded constants — \*\*\*
- `Unifier` — all methods return `false` / `kErrorType` — \*\*\*

### HIR (`src/zir/hir/`) \*\*
- Expression model (10 variants: literals, binary, unary, let, var, call, ret, branch, jump, phi) — \*
- Module storage — \*
- `addFn()` silently drops the function name (`(void)name`) — \*\*
- Verifier — returns `true` with no logic — \*\*\*

### MIR (`src/zir/mir/`) \*\*\*
- Instruction model (22 opcodes, operands, basic blocks) — \*
- Module storage — \*
- Lowering (`HIR → MIR`) — **no translation occurs** — \*\*\*
- Verifier — returns `true` with no logic — \*\*\*
- No instruction visitor — \*\*\*\*

### ZIR / Interpreter \*\*\*\*
Not yet started.

---

## Test Results

| Test | Status | Notes |
|------|--------|-------|
| `zithc-lexer-test` | \* PASS | 43/43 passing |
| `zithc-parser-expr` | \* PASS | AST builder tests |
| `zithc-parser-stmt` | \* PASS | AST builder tests |
| `zithc-sema-test` | \* PASS | Symbol table + basic sema context tests |
| `zithc-mir-test` | \* PASS | HIR module + basic lowering scaffold |
| `zithc-import-test` | \* PASS | Import resolution + symbol visibility tests |
| `zithc-import-e2e` | \* PASS | End-to-end import pipeline tests |

---

## Next Steps

See [NEXT_STEPS.md](NEXT_STEPS.md) for the prioritized roadmap.
