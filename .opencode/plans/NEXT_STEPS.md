# Next Steps — Zith Compiler

**Last updated:** 2026-06-20

Current target: **v0.2.0.0 — Real Parser**

---

## Current Status

- **27/27 tests passing** — clean compile
- Full lexer, AST builder, arena system — production-quality
- Parser: scan pass working, body expansion working, span computation on all AST nodes
- Pratt parser skeleton in place, most expressions parsing
- ZIR instruction model, emitter, and interpreter implemented (not yet wired into pipeline)
- `std::vector` usage identified for replacement in v0.3.0.0

---

## Priority Queue

### v0.2.0.0 — Real Parser

- [ ] Complete all expression types in Pratt parser (field, index, call, range, unary prefix)
- [ ] Complete statement parsing (let with types, assign, return)
- [ ] Complete declaration parsing in scan pass (struct fields, enum variants, union variants, trait/interface methods)
- [ ] Wire body expansion to produce real AST (not stubs)
- [ ] C-compatibility hooks in parser (`extern "C"` blocks, ABI attributes groundwork)
- [ ] Cleanup unnecessary `#include <vector>` in files that don't use it

### v0.3.0.0 — Type System + DynArray Purge

- [ ] `TypeIntern` deduplication — actual storage and structural comparison
- [ ] `Unifier` — type unification with error reporting
- [ ] `TypeLower` pass — TypeExpr → TypeId
- [ ] `std::vector` → `DynArray` in `src/zir/zir/` (zir-inst.hpp, zir-interp.hpp, zir-emitter.cpp)
- [ ] `std::vector` → `DynArray` in `src/diagnostics/` (diagnostic.hpp, diagnostic-engine.cpp)

### v0.3.5.0 — Full Sema

- [ ] `SemaPipeline` traverses AST → typed HIR
- [ ] Name resolution fully wired through resolver
- [ ] Generics/solver constraint checking
- [ ] HIR verifier validates expression types and CFG structure

### v0.4.0.0 — Macros

- [ ] Compile-time code transformation stage
- [ ] Macro declaration syntax
- [ ] Macro expansion during compilation
- [ ] Integration with sema pipeline

### v0.5.0.0 — Interpreter MVP

- [ ] HIR → MIR lowering (real translation, 22 opcodes)
- [ ] MIR verifier
- [ ] MIR → ZIR bytecode emission
- [ ] Wire ZIR interpreter into `CompilationSession::zirStage()`
- [ ] `zithc run --interpreted` works for single-file programs
- [ ] Native attempt displays: "Native support has not yet been added."

### v0.6.0.0 — LLVM Native Backend

- [ ] MIR → LLVM IR emission
- [ ] LLVM IR → object file
- [ ] `zithc compile` produces native binary
- [ ] Hello-world end-to-end

### v0.7.0.0 — Optimization Pipeline

- [ ] MIR → LLVM IR → LLVM opt → MIR custom target backend
- [ ] `-O1`/`-O2`/`-O3` flag integration
- [ ] Optimized interpreter path (opt → MIR' → ZIR → VM)
- [ ] ZIR → LLVM as secondary codegen path

### v0.8.0.0 — Production (Compiler Complete)

- [ ] Multi-file compilation + linking
- [ ] C-ABI compatibility (`extern "C"`, struct layout)
- [ ] Source-aware diagnostics (snippets, underlines)
- [ ] All 4 codegen paths complete
- [ ] **Compiler feature-complete** — all later versions are stdlib

---

## Quick Reference

```
v0.2.0.0  Parser
v0.3.0.0  Type System + DynArray
v0.3.5.0  Full Sema
v0.4.0.0  Macros
v0.5.0.0  Interpreter MVP
v0.6.0.0  LLVM Native
v0.7.0.0  Optimization Pipeline
v0.8.0.0  Production (compiler done)
v0.9.0.0+ Stdlib era
```

All test binaries: 27 registered, 27 passing.
