# Logic Changes Still Needed

These are bugs or missing implementations found during code review. Items
marked **[FIXED]** have been resolved in the current session.

---

## ~~1. `mapUnaryOp` Ref/Deref fall-through to Neg~~  **[FIXED]**

**File:** `src/sema/sema-pipeline.cpp:58-67`

Added `Ref`/`Deref` to `HirUnaryOp` enum and proper cases in `mapUnaryOp`.
Removed the fall-through `default` case.

---

## 2. Body Expansion only handles structs  [BUG]

**File:** `src/parser/parser.cpp:1755`

`expandBodies` iterates `result.structs` but ignores `result.enums`,
`result.unions`, and `result.traits`. Block bodies in enum/union/trait
declarations are never re-parsed.

**Note:** The struct body expansion (`parseStructBody`) is also effectively a
no-op — it reads tokens but never writes back to `StructDeclNode.fields`.
All AST type declarations have empty fields/variants/methods arrays. Sema
uses the symbol table (populated during scan), so basic lookups work.

**Fix:** Write `parseEnumBody`, `parseUnionBody`, `parseTraitBody` functions
and wire the results back to the AST nodes.

---

## 3. Field / Index / Range silently produce kInvalidHirExpr  [STUB]

**File:** `src/sema/sema-pipeline.cpp:191-194`

`FieldNode`, `IndexNode`, and `RangeNode` are matched but produce
`kInvalidHirExpr` — field access, indexing, and range expressions silently
generate no HIR.

**Fix:** Implement lowering for each.

---

## ~~4. Generic constraint verification is a no-op~~  **[FIXED]**

**File:** `src/solve/solver.cpp:135-141`

Changed `verifyGenericConstraints` to accept `HirModule&`, iterates the
actual module's functions, and calls `checkNumericBoundsInFn` on each.

---

## 5. `monomorphs_` declared but never populated  [STUB]

**File:** `src/solve/solver.hpp:26`

`memory::DynArray<MonomorphEntry> monomorphs_` exists but no code ever
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

## ~~9. Multi-name let ignores extra names~~  **[FIXED]**

**File:** `src/sema/sema-pipeline.cpp:478-481`

Changed the single-name declaration + HirLet emission into a loop over
all names in `n.names`.

---

## 10. Array count parser is a stub  [STUB]

**File:** `src/parser/parser.cpp:1442`

`parseArrayCount` returns 0 unconditionally. `[T; N]` type syntax is
parsed without actually reading `N`.

**Fix:** Parse the expression between `;` and `]`, evaluate it, and use
the result as the array count.

---

## 11. Trait `+` constraint not parsed  [STUB]

**File:** `src/parser/parser.cpp:1533-1536`

`parseTypeExpr` only calls `parseOrExpr` and ignores `+ Trait2 + Trait3`.
The comment says "requires trait system".

**Fix:** Implement when the trait system is in place.
