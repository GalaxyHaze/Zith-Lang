# {AGENT}: {TASK}

## Goal

Make every non-void function result `nodiscard` by default in Zith-- without
adding language syntax. A free call, method call, or other call used as an
expression statement must either discard its value explicitly with `_ = expr;`
or report a clear diagnostic.

## Context

- Plan: debt, discard-only expression statements and default `nodiscard`.
- Worktree: {REPO}/.awt/{AGENT} (branch awt/{AGENT}).
- Follow `docs/implementation-debt.md` under "Outras incompletudes registadas"
  and `docs/impl-status.md` under `Functions & Bindings` and `Known Debt`.
- The user explicitly rejected adding a `nodiscard` keyword or function
  attribute. All `fn` results are treated as consumed-or-discarded by default;
  the only explicit discard form is `_ = expr;`.
- Do not model full ownership or full-Zith owner tracking. Zith-- needs the
  lightweight rule described here only (`_ = discardValue();`).

## Scope

1. Diagnose expression statements whose root call returns a non-void type and
   is not consumed, assigned, discarded, or otherwise part of a value-producing
   expression.
2. Accept `_ = expr;` as the explicit discard. `_` is already known to the
   compiler as `ExprKind::Placeholder` in struct-literal paths; do not introduce
   a new lexer token unless a parser/sema slot is genuinely required.
3. Keep result-producing uses legal: binding initialization, assignment RHS,
   condition, return, argument, binary operand, call argument, field/index
   operand, and method dispatch all consume or use the value.
4. Keep `void` calls and non-call expression statements legal.

Owned files for implementation (choose the smallest surface):

- Parser/statement lowering if `_ = expr;` needs an AST shape:
  `src/frontend/frontend-expr.cpp`, `src/frontend/frontend-stmt.cpp`,
  `src/frontend/frontend.hpp`.
- Sema/statement check: `src/sema/sema-control.cpp`,
  `src/sema/sema-expr.cpp`, `src/sema/sema-modern.hpp`.
- Tests: `tests/test-frontend.cpp` and/or `tests/test-codegen.cpp`; do not add
  new executables.
- Docs: `docs/impl-status.md`, `docs/implementation-debt.md`, and the relevant
  Zith-- spec/implementation doc section. Edit only the section/line for this
  item in each shared file.

Out of scope:

- Adding a `nodiscard` keyword to `src/frontend/ast-lowerer.cpp`.
- Adding a `FunctionKind`, declaration field, or declaration attribute for
  discarded results.
- Full-NRA/move/owner analysis beyond the expression-statement discard check.
- `src/sema/sema-cast-coerce.cpp`, range files, pointer narrowing, and export
  namespace work; their agents own those files in this wave.
- Changing the return type checker or existing bindings/coercion semantics.

## Acceptance Criteria

- This program is rejected with a targeted diagnostic:

```zith
fn make(): i32 { return 1; }
fn main(): i32 { make(); return 0; }
```

- This program is accepted:

```zith
fn make(): i32 { return 1; }
fn main(): i32 { _ = make(); return 0; }
```

- `_ = make();` does not require a declared `_` binding, and a later `_ = make();`
  in the same block does not count as a duplicate declaration.
- `void` calls remain legal. Method calls and calls whose result is used in a
  binding, assignment, condition, return, argument, or expression stay legal.
- The rule is documented as Zith-- default behavior, not as an attribute.

## Verification

```bash
cmake --build {REPO}/build -j4
ctest --test-dir {REPO}/build -R 'frontend|codegen|sema|control|expr' --output-on-failure
cmake --build {REPO}/build --target fmt-check
ctest --test-dir {REPO}/build --output-on-failure
```

After success:

```bash
cd {REPO}/.awt/{AGENT}
{AWT_SKILL_DIR}/scripts/awt checkin {REPO} "{AGENT}: {TASK}"
{AWT_SKILL_DIR}/scripts/awt request-merge {REPO} "{AGENT}: {TASK}"
```

## End Of Task

After requesting merge, do NOT finish this session. Return to the listening
loop: re-read `TASK.md` whenever the scheduler advances this worktree. Execute
each new task as it appears and request merge again after completing it. Stop
only when `TASK.md` contains a `# Status` section with `end` below it.
