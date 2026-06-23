# Logic Changes Still Needed

These are bugs or missing implementations found during code review. Unlike the
refactors already completed (color system, comma-list helpers, spanFrom, etc.),
each of these changes *alters behavior* and needs design consideration before
switching from stub/no-op to real logic.

---

## 1. `mapUnaryOp` Ref/Deref fall-through to Neg  [BUG]

**File:** `src/sema/sema-pipeline.cpp:58-67`

`case ast::UnaryOp::Ref:` and `case ast::UnaryOp::Deref:` lack `break` statements
and fall through to `case ast::UnaryOp::Neg:`. `&x` and `*x` silently become `-x`.

**Fix:** Add a `break` (or better, emit a `HirUnary` with the correct op) for each case.

---

## 2. Body Expansion only handles structs  [BUG]

**File:** `src/parser/parser.cpp:1755`

`expandBodies` iterates `result.structs` but ignores `result.enums`,
`result.unions`, and `result.traits`. Block bodies in enum/union/trait
declarations are never re-parsed, making their fields/variants/methods
invisible to sema.

**Fix:** Add loops for `result.enums`, `result.unions`, `result.traits`
alongside the existing `result.structs` loop.

---

## 3. Field / Index / Range silently produce kInvalidHirExpr  [STUB]

**File:** `src/sema/sema-pipeline.cpp:191-194`

`FieldNode`, `IndexNode`, and `RangeNode` are matched but produce
`kInvalidHirExpr` — field access, indexing, and range expressions silently
generate no HIR.

**Fix:** Implement lowering for each.

---

## 4. Generic constraint verification is a no-op  [STUB]

**File:** `src/solve/solver.cpp:135-141`

`verifyGenericConstraints` iterates functions but never calls
`checkNumericBoundsInFn`. Generic numeric bounds are silently ignored.

**Fix:** Call `checkNumericBoundsInFn()` inside the loop.

---

## 5. `monomorphs_` declared but never populated  [STUB]

**File:** `src/solve/solver.hpp:26`

`memory::DynArray<MonoCandidate> monomorphs_` exists but no code ever
pushes to it. Monomorphization is completely missing.

**Fix:** Implement monomorph candidate collection in `Solver::collectGenerics`
or a new pass, then apply during lowering.

---

## 6. MIR / ZIR / LLVM pipeline stages are no-ops  [STUB]

**File:** `src/cli/compilation-session.cpp:378,390`

`solveStage()`, `lowerMirStage()`, and `codegenStage()` all immediately
`return true` (or print `"(stub)"` for the solver). The pipeline always
succeeds but produces nothing after HIR.

**Fix:** Wire up real MIR lowering, ZIR lowering, and LLVM codegen.

---

## 7. CLI commands are stubs  [STUB]

**Files:**
- `src/cli/cmd/run.cpp`
- `src/cli/cmd/deps.cpp`
- `src/cli/cmd/interactive.cpp`
- `src/cli/cmd/tool.cpp`
- `src/cli/cmd/project.cpp`

All use the identical pattern:
```
fmt::println("not implemented yet");
return 0;
```

**Fix:** Implement each command.

---

## 8. If/While control flow is flat (no branch/phi)  [STUB]

**File:** `src/sema/sema-pipeline.cpp:428-440`

`visitIf` and `visitWhile` evaluate the condition and bodies but do not
emit `HirBranch`/`HirJump`/`HirPhi` instructions. Control flow is
essentially linear — both branches are always taken sequentially.

**Fix:** Emit proper HIR control-flow instructions.

---

## 9. Multi-name let ignores extra names  [BUG]

**File:** `src/sema/sema-pipeline.cpp:478-481`

`visitLet` allocates a single `HirLet` for `names[0]` and ignores
`names[1..]`. `let a, b = expr` silently only introduces `a`.

**Fix:** Emit a `HirLet` for each name in the array.

---

## 10. Array count parser is a stub  [STUB]

**File:** `src/parser/parser.cpp:1442`

`parseArrayCount` returns 0 unconditionally. `[T; N]` type syntax is
parsed without actually reading `N`.

**Fix:** Parse the expression between `;` and `]`, evaluate it, and use
the result as the array count.

---

## 11. Trait `+` constraint not parsed  [STUB]

**File:** `src/parser/parser.cpp:1539`

`parseOptTraitConstraint` reads a single trait name and ignores any
following `+ Trait2 + Trait3`. Multi-trait bounds are silently truncated
to the first trait.

**Fix:** Loop while `consume('+')` and parse additional trait type exprs.
