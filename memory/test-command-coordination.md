# Native `zithc test`: split between master and hermes agents

This note records how the native `zithc test` work is divided across agents and
which files each side owns, so a future session can resume the coordination
without re-deriving it. The design itself is in `docs/plans/test-command.md`
and `docs/adr/0045-native-test-command-and-driver.md`; this note only covers who
builds what.

## Split

The master (this checkout, `main`) keeps the hard and risky steps. Hermes
worktrees take the long, mechanical steps. The plan is mostly serial, so the
real parallelism is only Wave 0 and the tail.

Master owns, in order:

- Step 3: HIR lowering of `@assert`/`@log` with the in-test versus out-of-test
  branch.
- Step 4: synthesis of the result table, the per-test log buffer, and the HIR
  driver.
- Step 5: the `Debug` section format and the location table.
- Steps 6 and 7: the rewritten `test()` command and the report modes.

hermes1 owns Wave 0 (frontend surface): `AttributeKind::Test`,
`classifyAttribute`, and the `assert`/`log` intrinsic names and parse branch.
Files: `src/frontend/frontend.hpp`, `src/frontend/attribute-classify.hpp`,
`src/frontend/ast-lowerer.hpp`, `src/frontend/frontend-expr-primary.cpp`.

hermes2 owns `std/test` (thin wrappers over `@assert`), then later the example
suite under `tests/zith/` and its CTest registration. Files:
`stdlib/std/test.zith` (new), later `tests/zith/*.zith` and `CMakeLists.txt`.

## File ownership boundary

This boundary prevents worktree collisions. The master keeps `src/sema/`,
`src/hir/`, `src/zirl/`, `src/session/`, `src/cli/`. hermes1 keeps
`src/frontend/` and `sema-cast-coerce.cpp`/`sema-modern.hpp` only if the master
has not yet touched them. hermes2 keeps `stdlib/std/test.zith`.

Shared files that need a single writer at merge time: `CMakeLists.txt` (CTest
registration) and any stdlib module list. The master merges those.

## Verification gates

1. Wave 0 to Wave 1: `zithc check` accepts `#[test] fn f(): void { @assert(true); }`,
   rejects `#[test]` on non-functions with `W2029`, and `fmt-check` is green.
2. Wave 1 to Wave 2: a failing `@assert` writes the correct result slot and the
   `Debug` section resolves the location; the C++ suite is green.
3. Wave 2 to Wave 3: `zithc test` runs the suite and reports per test on all
   three backends; the C++ suite and `fmt-check` are green.

## Worktree state

Worktrees live at `.awt/hermes1` (branch `awt/hermes1`) and `.awt/hermes2`
(branch `awt/hermes2`), both created from `d3978d54`. Each has a `TASK.md` and
an `agent-id`. `.awt/` is git-ignored, so checkin never commits them.

hermes1 (frontend surface) is merged into `main` as `84303dba`. Its branch
was rebased on `9e71b8ce` before the fast-forward merge. The merged commit adds
`AttributeKind::Test`, the `test` classify branch, the `assert`/`log` intrinsic
names, and the one-operand parse branch. Gate 0 to 1 passed: `zithc check`
accepts `#[test] fn f(): void { @assert(true); @log("x"); }`, `#[test]` on a
non-function is `W2029`, an unknown attribute is `W2028`, and `test-frontend`
is 402 passed / 0 failed. hermes2 (`std/test`) is not started yet.

There is older, unrelated `agent-scheduler` state under `.awt/` (manifest,
board, scheduler-status, and the merged `awt/agent1`..`awt/agent9` branches).
Those branches are all merged into `main`. Leave that state alone; the hermes
worktrees use distinct names on purpose.

## clang-format version gotcha

The repo is formatted with the CI's clang-format-18, but the local Homebrew
`clang-format` is 23.1.1. The two disagree on several baseline lines (for
example `ParserDepthGuard(const ParserDepthGuard &) = delete;`, the
`index_`/`parser_depth_` alignment block in `ast-lowerer.hpp`, and the two
`TokenKind::Dots` conditions in `frontend-expr-primary.cpp`). The baseline
`main` therefore already fails a local `clang-format --dry-run --Werror`, and
reformatting those lines with 23.1.1 makes the diff diverge from the repo
style. Do not reformat unrelated lines to satisfy the local binary; keep the
diff limited to the intended hunks. `fmt-check` is a local-only CMake target
and is not part of `.github/workflows/ci.yml`.
