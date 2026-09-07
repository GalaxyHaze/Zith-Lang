# Monolith Splits

## Objective

Reorganise compiler sources by responsibility without changing compiler
behaviour. The split targets large files that concentrate unrelated pipeline
stages, not language features. Each extraction must be mechanical, keep the
public APIs stable, and land with the same focused tests passing.

## Completed Splits

### `src/session/frontend-context.cpp`

The frontend-context monolith was split by responsibility and merged. The
pipeline entry point remains in `src/session/frontend-context.cpp` at
315 lines. The extracted translation units and current line counts are:

| Translation unit | Lines | Responsibility |
|---|---|---|
| `src/session/frontend-context.cpp` | 315 | public parsing/frontend orchestration entry point |
| `src/session/frontend-module-analysis.cpp` | 412 | module analysis state and discovery |
| `src/session/frontend-module-cache.cpp` | 275 | module cache bookkeeping |
| `src/session/frontend-source-catalog.cpp` | 196 | source catalog and fingerprinting helpers |
| `src/session/frontend-symbol-resolution.cpp` | 764 | import requests and symbol/module resolution |

### `src/session/compilation-session.cpp`

The compilation-session monolith was split by responsibility and merged.
`CompilationSession` orchestration remains in
`src/session/compilation-session.cpp` at 871 lines. The extracted units and
current line counts are:

| Translation unit | Lines | Responsibility |
|---|---|---|
| `src/session/compilation-session.cpp` | 871 | pipeline stage orchestration and session glue |
| `src/session/native-link.cpp` | 421 | native link/run helpers |
| `src/session/persistent-cache.cpp` | 721 | persistent/object cache helpers |
| `src/session/pipeline-plan.cpp` | 13 | planned pipeline stage contract |

## Remaining Candidates

Current priority files, based on `docs/implementation-debt.md`:

| File | Lines | Candidate split |
|---|---|---|
| `src/codegen/codegen-emit.cpp` | 1264 | emission by area (params, expr, control flow) |
| `src/sema/hir-lower-expr.cpp` | 2357 | secondary candidate; revisit if it still exceeds ~1000 lines after the codegen split |
| `src/frontend/frontend-expr.cpp` | 1115 | secondary candidate; revisit only if it still exceeds ~1000 lines after higher-priority work |

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

`frontend-context.cpp` and `compilation-session.cpp` are merged. The next
candidate is:

1. `src/codegen/codegen-emit.cpp`: split emission by expression types, calls,
   statements/blocks and aggregate helpers.
2. Revisit `src/sema/hir-lower-expr.cpp` only if it still exceeds ~1000 lines
   after the codegen split.
3. Revisit `src/frontend/frontend-expr.cpp` only if it remains a clear
   single-responsibility bottleneck; it is currently a secondary candidate.

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
