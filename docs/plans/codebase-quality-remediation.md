# Codebase Quality Remediation Plan

Objective: bound recursive parser work and add memory/undefined-behavior
sanitizer coverage for the compiler tests, following
[`codebase-audit-2026-10-15.md`](../codebase-audit-2026-10-15.md).

Status: planned. No source-code or CI remediation in this plan has been
implemented yet.

Preconditions:

- Work from `/home/diogo/Zith`.
- Preserve pre-existing modifications. The repository may have a dirty
  worktree.
- Use the configured CMake/CTest project and Clang 18+ used by
  `.github/workflows/ci.yml`.
- Run `git status --short` before each implementation step. If a target file
  already has user changes, read its diff and build on it without reverting.

Files expected to change:

- `src/frontend/ast-lowerer.hpp`
- `src/frontend/frontend-expr.cpp`
- `src/frontend/frontend-expr-primary.cpp`
- `src/frontend/frontend-types.cpp`
- `src/frontend/frontend-stmt.cpp`
- `src/diagnostics/error-codes.hpp`
- `src/diagnostics/error-codes.cpp`
- `tests/test-frontend.cpp`
- `docs/impl-status.md`
- `.github/workflows/ci.yml`

## Steps

### 1. Bound parser recursion

Goal: deeply nested source produces a stable parser diagnostic instead of
recursing until process stack exhaustion.

Sub-steps:

1. Run `git status --short` from `/home/diogo/Zith` and inspect any existing
   changes to the target files before editing.
2. Inspect the recursive parsing entry points in
   `src/frontend/frontend-expr.cpp`,
   `src/frontend/frontend-expr-primary.cpp`,
   `src/frontend/frontend-types.cpp` and `src/frontend/frontend-stmt.cpp`.
   Include `parseExpression`, `parseType`, block parsing and recursive
   conditional parsing in the depth accounting.
3. Add a shared RAII depth guard to `AstLowerer` in
   `src/frontend/ast-lowerer.hpp`. Set the maximum active parser depth to 128.
   Ensure every return path decrements the depth automatically.
4. Reserve parser diagnostic code 1010 as `ParseDepthExceeded` in
   `src/diagnostics/error-codes.hpp`. Add its user-facing category, title and
   suggestion in `src/diagnostics/error-codes.cpp`.
5. At the first over-limit parser entry, emit one `E1010`, return the relevant
   error node/id, and recover iteratively to the matching delimiter or parser
   synchronization point. Do not recursively parse while recovering.
6. Add deterministic tests to `tests/test-frontend.cpp` for nested parenthesized
   expressions, nested types, and nested blocks/conditionals. Generate each
   input in a loop so the test source remains small. Assert that depth 128 is
   accepted where the grammar permits it, and depth 129 reports `E1010`
   without aborting or hanging.
7. Update the parser error-code range/table in `docs/impl-status.md` to include
   `E1010`.
8. Build and run the focused frontend test.

Commands:

```bash
cd /home/diogo/Zith
cmake --build build --target test-frontend -j4
ctest --test-dir build -R '^test-frontend$' --output-on-failure
```

Expected output: the target builds successfully and CTest reports
`100% tests passed` for `test-frontend`.

Failure checks:

- If nesting at depth 128 fails, verify whether the guard counts parser entry
  frames or source nesting levels. Keep the documented boundary at 128 source
  nesting levels and adjust only the guard accounting.
- If depth 129 crashes or hangs, the guard misses a recursive entry point or
  recovery recurses. Trace the parse call path and add that entry point to the
  same guard; keep recovery iterative.
- If `E1010` is already assigned when implementation begins, stop and inspect
  `src/diagnostics/error-codes.hpp`, `error-codes.cpp` and
  `docs/impl-status.md`; select the next unused parse code consistently rather
  than reusing a diagnostic.
- If the focused test fails for a syntax reason unrelated to depth, print the
  generated source and diagnostic, correct the test fixture, and do not weaken
  the limit assertion.

Success criteria: `test-frontend` exits 0; the exact over-limit cases return
`E1010`; accepted boundary cases remain accepted; no recursive recovery path
is introduced.

### 2. Add an ASan/UBSan CI run

Goal: the regular test suite runs in a Debug build instrumented with AddressSanitizer
and UndefinedBehaviorSanitizer.

Sub-steps:

1. Read the existing `build` job in
   `/home/diogo/Zith/.github/workflows/ci.yml`. Reuse its LLVM installation,
   compiler selection and configure options.
2. Add a separate `sanitizers` job on `ubuntu-latest`. Keep the existing
   Debug/Release matrix unchanged.
3. Configure a fresh `build-sanitized` directory with Clang 18, LLVM required,
   Debug mode, and `-fsanitize=address,undefined -fno-omit-frame-pointer` in
   both C and C++ compile flags and both executable and shared linker flags.
   Do not reuse `build/` because it contains non-instrumented objects.
4. Build all targets in `build-sanitized`.
5. Run the full CTest suite with `--output-on-failure`. Do not mark the job
   `continue-on-error` or `allow-failure`.
6. Confirm that failures include sanitizer reports in the job output and fix
   any confirmed defect before merging the CI job.

Commands for the job:

```bash
cmake -S . -B build-sanitized \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang-18 \
  -DCMAKE_CXX_COMPILER=clang++-18 \
  -DZITH_HAS_LLVM=ON \
  -DZITH_REQUIRE_LLVM=ON \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
  -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-sanitized -j"$(nproc)"
ctest --test-dir build-sanitized --output-on-failure
```

Expected output: the sanitizer build succeeds and CTest reports all registered
tests passing or explicitly skipped by their existing skip contract.

Failure checks:

- If configuration cannot find LLVM, verify that the sanitizer job installs the
  same LLVM 18 development packages as the existing native CI job. Keep
  `ZITH_REQUIRE_LLVM=ON`.
- If the build links but sanitizer runtimes are absent at execution, verify the
  compiler and linker flags are present in the generated compile/link commands.
- If CTest reports an ASan or UBSan finding, keep the sanitizer job failing,
  reproduce it locally, and fix the underlying issue before changing the job.
- If a pre-existing test is incompatible with sanitizers, document the exact
  incompatibility and add a narrowly scoped test property only after confirming
  that the test itself cannot be made sanitizer-clean.

Success criteria: the new CI job executes the complete CTest suite under both
sanitizers and is a required successful job for pull requests.

## Final Acceptance

After both steps are implemented:

1. Run `cmake --build build --target test-frontend -j4`.
2. Run `ctest --test-dir build --output-on-failure`.
3. Run the sanitizer job's exact configure, build and CTest commands locally
   when LLVM 18 is available.
4. Run `python3 scripts/check-docs-consistency.py --root /home/diogo/Zith`.
5. Review `git diff --check` and inspect the complete diff. Confirm unrelated
   pre-existing changes remain untouched.

The work is complete only when the focused test, full local CTest suite,
documentation check and required CI sanitizer job all pass.
