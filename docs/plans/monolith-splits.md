# Monolith Splits

## Objective

Reorganise compiler sources by responsibility without changing compiler
behaviour. The split targets large files that concentrate unrelated pipeline
stages, not language features. Each extraction must be mechanical, keep the
public APIs stable, and land with the same focused tests passing.

## Completed Splits

### `src/session/frontend-context.cpp`

The frontend-context monolith was split by responsibility and merged. The
pipeline entry point remains in `src/session/frontend-context.cpp`. The
current line counts, refreshed on 2026-10-15 with `wc -l`, are:

| Translation unit | Lines | Responsibility |
|---|---|---|
| `src/session/frontend-context.cpp` | 330 | public parsing/frontend orchestration entry point |
| `src/session/frontend-module-analysis.cpp` | 451 | module analysis state and discovery |
| `src/session/frontend-module-cache.cpp` | 275 | module cache bookkeeping |
| `src/session/frontend-source-catalog.cpp` | 196 | source catalog and fingerprinting helpers |
| `src/session/frontend-symbol-resolution.cpp` | 902 | import requests and symbol/module resolution |

### `src/session/compilation-session.cpp`

The compilation-session monolith was split by responsibility and merged.
`CompilationSession` orchestration remains in
`src/session/compilation-session.cpp`. The current line counts, refreshed on
2026-10-15 with `wc -l`, are:

| Translation unit | Lines | Responsibility |
|---|---|---|
| `src/session/compilation-session.cpp` | 882 | pipeline stage orchestration and session glue |
| `src/session/native-link.cpp` | 403 | native link/run helpers |
| `src/session/persistent-cache.cpp` | 788 | persistent/object cache helpers |
| `src/session/pipeline-plan.cpp` | 13 | planned pipeline stage contract |

### `src/codegen/codegen-emit.cpp`

The codegen monolith was split by responsibility and merged. The class and
orchestration remain in `src/codegen/codegen-emit.cpp`; the extracted units
and current line counts, refreshed on 2026-10-15 with `wc -l`, are:

| Translation unit | Lines | Responsibility |
|---|---|---|
| `src/codegen/codegen-emit.cpp` | 9 | class and orchestration |
| `src/codegen/codegen-emit-expr.cpp` | 1029 | expression emission |
| `src/codegen/codegen-emit-stmt.cpp` | 162 | statements and control flow |
| `src/codegen/codegen-emit-agg.cpp` | 138 | aggregate helpers |

### `src/sema/hir-lower-expr.cpp`

The HIR expression lowering monolith was split by responsibility and merged.
The public dispatcher remains in `src/sema/hir-lower-expr.cpp`. The current
line counts, refreshed on 2026-10-15 with `wc -l`, are:

| Translation unit | Lines | Responsibility |
|---|---|---|
| `src/sema/hir-lower-expr.cpp` | 121 | `lowerExpr` dispatcher and public glue |
| `src/sema/hir-lower-expr-value.cpp` | 1703 | value lowering, coercions, literals, names, casts, pipelines, optional payloads |
| `src/sema/hir-lower-expr-access.cpp` | 392 | lvalue/address, index, slice range, field, and arrow lowering |
| `src/sema/hir-lower-expr-agg.cpp` | 311 | enum variant, struct, pack, array, and field default lowering |

### `src/frontend/frontend-expr.cpp`

The frontend expression parser was split by responsibility and merged. The
expression dispatcher and call argument parsing remain in
`src/frontend/frontend-expr.cpp`; primary/postfix parsing moved to
`src/frontend/frontend-expr-primary.cpp` and operator/precedence helpers moved
to `src/frontend/frontend-expr-operator.cpp`. Current line counts, refreshed
on 2026-10-15 with `wc -l`, are:

| Translation unit | Lines | Responsibility |
|---|---|---|
| `src/frontend/frontend-expr.cpp` | 325 | dispatcher and call arguments |
| `src/frontend/frontend-expr-primary.cpp` | 742 | primary and postfix parsing |
| `src/frontend/frontend-expr-operator.cpp` | 141 | operators and precedence |

## Remaining Candidates

No split is approved or active. Large translation units remain, including
`src/sema/hir-lower-expr-value.cpp` at 1703 lines. Treat file size as a review
signal, not as sufficient justification for another split. Approve a new split
only when a concrete cohesion, reviewability, or build-time problem is
identified.

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

`frontend-context.cpp`, `compilation-session.cpp`, `codegen-emit.cpp`,
`hir-lower-expr.cpp`, and `frontend-expr.cpp` are merged. No remaining
candidate is active.

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
