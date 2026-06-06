# Zith Compiler — Implementation Status

**Branch:** rewrite  
**Date:** 2026-06-06  
**Build:** `cmake -B build && cmake --build build` — clean compile, 33 targets  
**Tests:** `ctest --test-dir build` — 5/5 passing

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
         *        ***     *     ***    **     ***    ****      ****
```

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

### CLI (`src/cli/`) \*\*

| Component | Status | Notes |
|-----------|--------|-------|
| Argument parsing (`parseArgs`) | \* | 21 flags, 13 subcommands, full validation |
| `ZithFlags.toml` loader | \* | Reads default config from compiler-relative path |
| Config merging | \* | CLI flags → ZithFlags.toml cascade |
| `cmd_help`, `cmd_version` | \* | Ported from master |
| `cmd_new` | \* | Full project scaffolding |
| `cmd_check` | \*\* | Runs frontend pipeline, reports pass/fail |
| `cmd_compile` | \*\* | Chains via check → codegen (stub) |
| `cmd_build` | \*\* | Chains via compile → link (stub) |
| `cmd_run` | \*\* | Chains via build → execute (stub) |
| `cmd_execute` | \*\*\* | Recognises target, execution stubbed |
| `cmd_fmt` | \*\*\* | Validates parse, no re-indent yet |
| `cmd_test` | \*\*\* | Not implemented |
| `cmd_docs` | \*\*\* | Not implemented |
| `cmd_repl` | \*\*\* | Not implemented |
| `cmd_clean` | \*\*\* | Not implemented |
| Pipeline stages (`CompilationSession`) | \*\*\* | All 5 stages are pass-through stubs |

### ZithProject.toml integration \*\*\*
Not yet wired in. `main()` has a TODO placeholder.

### Parser (`src/parser/`) \*\*\*
- Class hierarchy designed with Pratt-style expression parsing — \*\*
- **Actual parsing:** all methods return `kInvalid*` / `false` — \*\*\*

### Symbol Table (`src/import/`) \*\*\*
- Scope enter/exit — \*
- `declare()`, `lookup()`, `lookupInScope()` — \*\*\*
- Name resolver (`resolveProgram`, etc.) — \*\*\*

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
| `zith-lexer-test` | \* PASS | 41/41 passing |
| `zith-parser-expr` | \*\*\*\* FAIL | Parser is a stub |
| `zith-parser-stmt` | \*\*\*\* FAIL | Parser is a stub |
| `zith-sema-test` | \*\*\*\* FAIL | Type intern/unify are stubs |
| `zith-mir-test` | 💥 SEGFAULT | MirLowering::lower() returns empty module |

---

## Next Work

### Short-term (CLI)
1. **Wire up ZithProject.toml** — port project-config from master, merge into `main()`
2. **Implement `cmd_clean`** — remove build artifacts (`target/`, `.zcache`, `.zmodules`)
3. **Implement `cmd_fmt`** — full source formatter with re-indent and normalisation
4. **Implement `cmd_execute`** — native fork+exec and interpreted bytecode runner

### Medium-term (core compiler)
5. **Implement the parser** — recursive-descent with Pratt expression parsing
6. **Implement the type interner** — actual storage, dedup, kindOf
7. **Implement name resolution** — symbol table declare/lookup
8. **Wire up sema pipeline** — type checking and HIR lowering
9. **Implement MIR lowering** — HIR → MIR translation
10. **Verifiers** — HIR and MIR verification passes
11. **ZIR interpreter / LLVM backend**
