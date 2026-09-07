# Monolith Splits

## Objective

Reorganise compiler sources by responsibility without changing compiler
behaviour. The split targets large files that concentrate unrelated pipeline
stages, not language features. Each extraction must be mechanical, keep the
public APIs stable, and land with the same focused tests passing.

## Scope

Current priority files, based on `docs/implementation-debt.md`:

| File | Lines | Candidate split |
|---|---|---|
| `src/session/frontend-context.cpp` | ~1789 | cache/module executor, module analysis, symbol resolution |
| `src/session/compilation-session.cpp` | ~1985 | pipeline stages, link/exec, cache |
| `src/codegen/codegen-emit.cpp` | ~1206 | emission by area (params, expr, control flow) |
| `src/sema/hir-lower-expr.cpp` | ~2140 | secondary candidates below 1000 lines |
| `src/frontend/frontend-expr.cpp` | ~1077 | secondary candidates below 1000 lines |

Earlier work already split `frontend.cpp` into AST/CST lowering,
frontend types, expressions, statements and declarations; `sema-modern.cpp`
into decl/type/expr/call/method/control/cast/assign/index/literal/state/zith
units; and HIR lowering into types/expr/call/block/stmt/util units.

## Execution Contract

1. Each extraction starts from the current behaviour. Do not add language
   features in the same commit.
2. Move code and private helpers into a TU named for the responsibility. Keep
   the original public class/API surface intact.
3. Re-run CMake after adding a new `.cpp` because `CMakeLists.txt` globs
   `src/*.cpp`.
4. Build `zithcLib` plus the focused tests for the affected area.
5. Run `ctest --test-dir build --output-on-failure` before closing.
6. Follow `.clang-format`; run `fmt-check` on touched files or format the whole
   tree only after user changes are committed or known.

## Order

1. `src/session/frontend-context.cpp`: split module cache/executor,
   module-analysis state, and symbol/import resolution. The file currently
   mixes fingerprinting, source catalogs, module discovery, import requests,
   cache bookkeeping and scoped symbol compilation.
2. `src/session/compilation-session.cpp`: keep `CompilationSession` orchestration
   and extract object-cache, native link/run, and CLI-facing helpers.
3. `src/codegen/codegen-emit.cpp`: split emission by expression types, calls,
   statements/blocks and aggregate helpers.
4. Revisit `src/sema/hir-lower-expr.cpp` and `src/frontend/frontend-expr.cpp`
   only if they still exceed ~1000 lines after the first passes.

Recommended first merge: one behavior-preserving extraction from
`frontend-context.cpp`. It is independent from the `drop` feature work, so it
can also be used as a low-risk task while `drop` is being designed.

## Success Criteria

- No public header or call-site change outside the extracted TU.
- Full build with `-Weverything -Werror` under Clang.
- Focused tests and full CTest pass with the same status as before the split.
- No new `std::printf`, `std::cerr`, or `fprintf(stderr, ...)` debug output.
- The work is reported as a merge-only, behavior-preserving refactor.

## Relationship To Other Plans

This is a Zith infrastructure track and does not depend on the Zith vs
`Zith--` feature split. Monolith splits can proceed in parallel with `drop`
(Zith--), C toolchain work, or the archived full-Zith comptime plan.
