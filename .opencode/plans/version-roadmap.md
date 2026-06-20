# Zith Compiler — Version Roadmap

**Version scheme:** `0.x.y.z` where:
- `x` = major phase/step
- `y` = significant progress within a phase
- `z` = small changes, fixes

**Target:** v0.8.0.0 = compiler feature-complete; all later versions are stdlib.

---

## Codegen Architecture

```
                    ┌──→ LLVM IR ──→ LLVM opt ──→ native binary
                    │                          └──→ MIR' → ZIR → VM
MIR ────────────────┤
                    ├──→ ZIR → VM                            (opt off)
                    └──→ LLVM IR → native binary             (opt off)
```

- MIR is the full-info IR, one step to LLVM via LLVM IR
- LLVM serves as the optimizer for **both** paths:
  - Native: LLVM IR → LLVM opt → native binary
  - Interpreted: LLVM IR → LLVM opt → MIR' (custom LLVM target) → ZIR → VM
- No opt: MIR → ZIR (interpreter) or MIR → LLVM IR → native
- ZIR is VM-optimized bytecode

---

## Version Plan

### 0.2.0.0 — Real Parser
- Complete Pratt parser (all expressions, statements, declarations)
- Body expansion produces real AST
- C-compatibility hooks (`extern` blocks, ABI groundwork)
- `<vector>` include cleanup in files that don't use it

### 0.3.0.0 — Type System + DynArray Purge
- `TypeIntern` deduplication (real storage, structural comparison)
- `Unifier` with error reporting
- `TypeLower` pass (TypeExpr → TypeId)
- `std::vector` → `DynArray` in:
  - `src/zir/zir/` (zir-inst.hpp, zir-interp.hpp, zir-emitter.cpp)
  - `src/diagnostics/` (diagnostic.hpp, diagnostic-engine.cpp)

### 0.3.5.0 — Full Sema
- `SemaPipeline` traverses AST → typed HIR
- Name resolution fully wired
- Generics/solver constraints
- HIR verifier

### 0.4.0.0 — Macros
- Compile-time code transformation
- Macro expansion stage (before or during sema)
- Macro declaration and invocation syntax

### 0.5.0.0 — Interpreter MVP
- HIR → MIR lowering
- MIR verifier
- MIR → ZIR bytecode emission
- ZIR interpreter (stack-based VM)
- `zithc run --interpreted` works for single-file programs
- Native attempt → `"Native support has not yet been added."`

### 0.6.0.0 — LLVM Native Backend
- MIR → LLVM IR emission
- LLVM IR → object file
- `zithc compile` produces native binary
- Hello-world works end-to-end, no optimization

### 0.7.0.0 — Optimization Pipeline
- MIR → LLVM IR → LLVM opt → MIR custom target backend
- `-O1`/`-O2`/`-O3` flag integration
- Optimized interpreter path (opt → MIR' → ZIR → VM)
- ZIR → LLVM as secondary codegen path

### 0.8.0.0 — Production (Compiler Complete)
- Multi-file compilation + linking
- C-ABI compatibility (`extern "C"`, struct layout)
- Source-aware diagnostics (snippets, underlines)
- All 4 codegen paths complete
- **Compiler feature-complete** — all future versions are stdlib

### 0.9.0.0+ — Stdlib Era
- Full standard library
- Package manager
- Tooling (formatter, LSP, test runner)

---

## Pipeline Stages

```
Source → Lexed → Scanned → Imported → Resolved → TypeChecked → Solved → HirLowered → MirLowered → ZirInterpreted
```

- Sema combines type-checking + HIR lowering (produces incomplete HIR naturally)
- Solver handles generic instantiation and monomorphization
- Codegen (LLVM/ZIR) is post-MIR
