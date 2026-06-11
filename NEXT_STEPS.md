# Next Steps — Zith Compiler

**Last updated:** 2026-06-11

This document outlines the prioritized next steps for the Zith compiler rewrite.  
Each phase builds on the previous one — complete items in order for the most efficient path to a working compiler.

---

## Phase 0 — Quick Wins (low effort, high impact)

### 0.1 Fix clang-format violations
- `cmake --build build -t fmt` to auto-format all source files
- Then verify with `cmake --build build -t fmt-check`
- **Why:** CI will reject PRs that aren't formatted. Do this first.

### 0.2 Implement `cmd_clean`
- Remove `target/`, `.zcache`, `.zmodules` build artifacts
- Single-file change in `src/cli/cmd/`

### 0.3 Implement `cmd_fmt`
- Full source formatter with re-indent and normalisation
- Parser+printer based — needs the real parser (Phase 2), so defer if needed

---

## Phase 1 — Project Config & CLI Polish

### 1.1 Wire up `ZithProject.toml`
- Port project-config from `master` branch
- Merge into `main()` before pipeline execution
- Config struct already exists in `src/cli/project-config.hpp` (stub)

### 1.2 Implement `cmd_execute`
- Native fork+exec after compilation
- Bytecode runner for interpreted mode (post-ZIR)

### 1.3 Remaining CLI commands
- `cmd_test` — test runner
- `cmd_docs` — doc generator
- `cmd_repl` — interactive REPL

---

## Phase 2 — Core Compiler: Parser

### 2.1 Implement the parser
- Recursive-descent with Pratt expression parsing (design already in place)
- All 8 expression types, 3 statement types, 3 declaration types
- Parse real `.zith` files end-to-end

### 2.2 Wire parser into pipeline
- Replace `kInvalid*` stubs in `CompilationSession::parseStage()`

### 2.3 Enable parser tests
- `zithc-parser-expr` and `zithc-parser-stmt` should test real parsing, not just AST builder

---

## Phase 3 — Type System

### 3.1 Implement `TypeIntern` properly
- Actual storage and deduplication of types
- `kindOf()`, `isType()`, structural comparison

### 3.2 Implement `Unifier`
- Type unification for inference and checking
- Error reporting on type mismatch

---

## Phase 4 — Name Resolution & Semantic Analysis

### 4.1 Implement name resolution
- Wire up symbol table `declare()`/`lookup()` in the resolver
- Module scope chaining

### 4.2 Wire up Sema pipeline
- `SemaPipeline::run()` should traverse AST and produce typed HIR
- Type checking, name resolution, HIR lowering all in one pass

---

## Phase 5 — HIR & MIR

### 5.1 Fix HIR `addFn()` — stop dropping function names
- One-line fix in `hir-module.cpp`

### 5.2 Implement verifiers
- HIR verifier — validate expression types, CFG structure
- MIR verifier — validate operands, basic block links

### 5.3 Implement MIR lowering
- `MirLowering::lower()` should translate HIR → MIR
- 22 opcodes, operands, basic blocks

---

## Phase 6 — Code Generation

### 6.1 ZIR / Interpreter
- Portable intermediate format
- Interpreted bytecode runner

### 6.2 LLVM backend
- Native code generation via LLVM
- Multi-target (Linux, macOS, Windows, WASM)

---

## Phase 7 — Quality & Polish

### 7.1 Source-aware diagnostics
- Emitter should show source snippets with underlines
- `diagnostics::emit.cpp` — the main file to change

### 7.2 Test coverage
- Property-based testing for lexer, parser
- Fuzz testing for the full pipeline

### 7.3 Performance
- Profile arena allocation, lexer hot paths
- Parallelize pipeline stages where possible

---

## Quick Reference

```text
Phase 0:  Format → cmd_clean → cmd_fmt
Phase 1:  ZithProject.toml → cmd_execute → remaining CLI
Phase 2:  Parser implementation
Phase 3:  TypeIntern → Unifier
Phase 4:  Name resolution → Sema pipeline
Phase 5:  HIR fix → Verifiers → MIR lowering
Phase 6:  ZIR interpreter → LLVM backend
Phase 7:  Diagnostics → Tests → Performance
```

Current test suite: **7/7 passing**  
Current build: **clean compile**
