# Codebase Quality Remediation Plan

Objective: bound recursive parser work and add memory/undefined-behavior
sanitizer coverage for the compiler tests, following
[`codebase-audit-2026-10-15.md`](../codebase-audit-2026-10-15.md).

Status: steps 1 and 2 are implemented and locally verified. Step 3 is complete:
the Clang 18 sanitizer job passed remotely for the published remediation head
SHA. The overall workflow still fails in the unrelated MUSL cross-build jobs;
see the remote verification record below. On 2026-10-15, GitHub reported
`main` as unprotected.

Preconditions:

- Work from `/home/diogo/Zith`.
- Preserve pre-existing modifications. The repository may have a dirty
  worktree.
- Keep step 1 in commit `266d2147` unchanged. Make step 2 a separate commit.
- Do not push additional commits to `main` without explicit user authorization.
- Use the configured CMake/CTest project. CI pins Clang 18, while the local
  sanitizer verification recorded below used Clang 21.
- Run `git status --short` before each implementation step. If a target file
  already has user changes, read its diff and build on it without reverting.

Files changed by step 1 in commit `266d2147`:

- `src/frontend/ast-lowerer.hpp`
- `src/frontend/frontend-expr.cpp`
- `src/frontend/frontend-expr-primary.cpp`
- `src/frontend/frontend-types.cpp`
- `src/frontend/frontend-stmt.cpp`
- `src/diagnostics/error-codes.hpp`
- `src/diagnostics/error-codes.cpp`
- `tests/test-frontend.cpp`
- `docs/impl-status.md`

Files changed by step 2:

- `.github/workflows/ci.yml`
- `src/sema/sema-control.cpp`
- `src/sema/hir-lower-block.cpp`
- `src/session/compilation-session.hpp`
- `src/session/compilation-session.cpp`
- `tests/test-interface-satisfaction.cpp`
- `tests/test-vm-v2.cpp`
- `docs/plans/codebase-quality-remediation.md`
- `memory/README.md`
- `memory/parser-depth-limit.md`
- `memory/sanitizer-ci.md`
- `memory/session-arena-lifetime.md`

Files updated by step 3:

- `docs/plans/codebase-quality-remediation.md`
- `memory/README.md`
- `memory/sanitizer-ci.md`

## Steps

### 1. Bound parser recursion (DONE, commit `266d2147`)

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

Recorded result: the focused frontend test passed, the over-limit cases return
`E1010`, accepted boundary cases remain accepted, and recovery remains
iterative. The completed change is isolated in commit `266d2147`.

Success criteria: DONE. Do not amend or rewrite commit `266d2147` while
completing step 2.

### 2. Add an ASan/UBSan CI run (IMPLEMENTED, locally and remotely verified)

Goal: the `sanitizers` GitHub Actions job runs all CTest cases in a Debug build
instrumented with AddressSanitizer and UndefinedBehaviorSanitizer.

Sub-steps:

1. Inspect the existing `build` job in
   `/home/diogo/Zith/.github/workflows/ci.yml`. Keep the existing Debug/Release
   matrix unchanged.
2. Add a separate `sanitizers` job on `ubuntu-latest`. Install CMake,
   `clang-18`, `lld-18`, `llvm-18-dev`, `libclang-18-dev` and `pkg-config`.
3. Configure a fresh `build-sanitized` directory in Debug mode with Clang 18,
   `ZITH_HAS_LLVM=ON`, `ZITH_REQUIRE_LLVM=ON`, ASan and UBSan compile/link
   flags, and frame pointers. Do not reuse `build/`.
4. Run `python3 scripts/verify-llvm-build.py --build-dir build-sanitized`.
   Stop if it reports a sema-only or LLVM-disabled build.
5. Build every target in `build-sanitized`.
6. Run the full CTest suite with `--output-on-failure`. Do not set
   `continue-on-error` or an allow-failure condition.
7. Add a regression in `tests/test-interface-satisfaction.cpp` that compiles
   an `if` without `else` through `session::Stage::HirLowered`.
8. In `src/sema/sema-control.cpp`, check that the if-expression has an
   else-operand before indexing that operand. In
   `src/sema/hir-lower-block.cpp`, read the else-operand only when `has_else`
   is true.
9. Keep `mScratchArena` declared before `mProjectConfig` in
   `src/session/compilation-session.hpp` and initialize the members in that
   same order in `src/session/compilation-session.cpp`. This keeps the arena
   alive while the configuration uses it.
10. In `tests/test-vm-v2.cpp`, pass `ZITH_STDLIB_DIR` instead of the
    CTest-working-directory-relative path `"stdlib"`.
11. Run the normal and sanitizer full suites and the focused tests. Record
    compiler versions, pass counts and environment limitations in
    `memory/sanitizer-ci.md` and `memory/parser-depth-limit.md`.
12. Record the member-lifetime dependency in
    `memory/session-arena-lifetime.md`, link the notes from
    `memory/README.md`, validate docs and workflow syntax, then commit step 2
    separately from `266d2147`.

CI commands:

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

- If CMake cannot find LLVM, verify that all LLVM 18 development packages are
  installed and keep `ZITH_REQUIRE_LLVM=ON`.
- If the verifier detects LLVM disabled, stop. Do not accept a sema-only
  sanitizer build.
- If sanitizer runtimes are missing, inspect generated compile/link commands
  and ensure the C and C++ compiler selections and sanitizer flags match.
- If CTest reports ASan or UBSan, keep the job failing, reproduce the finding,
  and fix the underlying defect. Do not add an allow-failure condition.
- If the no-else regression fails before HIR lowering, inspect its diagnostics
  and stage boundary. Keep `HirLowered` as the target stage.
- If `test-vm-v2` cannot find `stdlib`, use the configured
  `ZITH_STDLIB_DIR`. Do not rely on the test working directory.
- If local LeakSanitizer reports that it cannot run under `ptrace`, set
  `ASAN_OPTIONS=detect_leaks=0` only for local tests and record the limitation.
  Keep leak detection enabled in CI.
- If the sanitizer helper selects `/usr/bin/cc` and cannot link its sanitizer
  runtime, configure a fresh CMake build with both C and C++ compilers set to
  Clang. A configure/build failure is not sanitizer test coverage.

Recorded local result: the regular CTest suite passed 42/42. ASan/UBSan CTest
also passed 42/42 in a fresh Clang 21 build with
`ASAN_OPTIONS=detect_leaks=0`. LeakSanitizer cannot run under local `ptrace`,
so this run does not validate leaks. The focused `test-c-compile`,
`test-vm-v2` and `test-codegen` tests passed. `clang-tidy` reported no findings
in changed C++ files. `cppcheck` reported existing findings outside the
changed areas. `scripts/check-docs-consistency.py` passed. Ruby's standard YAML
parser accepted `.github/workflows/ci.yml`; `actionlint` is not installed.

Success criteria: local implementation acceptance is DONE. Remote sanitizer
acceptance is DONE after the successful Clang 18 run recorded below. Whether
branch protection requires this job is a separate repository-host setting.
The 2026-10-15 GitHub API query returned `404 Branch not protected`, so
`main` did not have required status checks configured at that time.

### 3. Verify the remote sanitizer job (COMPLETE)

Goal: verify the Clang 18 sanitizer workflow against the published remediation
commits and record the branch-protection state.

Recorded remote result on 2026-10-15:

- Run `38798317687` completed against head SHA
  `29366f79691df1d4a20e2f23e3d82cfe890715a2`, which contains the remediation
  commits `266d2147` and `b6e55e97`.
- The `sanitizers` job passed. It configured with ASan and UBSan, built
  successfully, and passed all 42 CTest tests with zero failures.
- `build (Debug)` and `build (Release)` also passed.
- The overall workflow conclusion was `failure`, not success.
  `cross-builds (web, false)` failed and the MUSL step in
  `cross-builds (wasm-runtime, true)` failed, both with undefined LLVM C++
  link symbols. The wasm-runtime job was cancelled after its MUSL step failed.
  This cross-link issue is separate from the sanitizer result and also
  appeared in earlier runs.
- The required-status-checks endpoint returned `404 Branch not protected` for
  `main`. No required status checks were configured at this snapshot.
- Local `main` was synchronized with `origin/main` at
  `29366f79691df1d4a20e2f23e3d82cfe890715a2` before this verification-record
  update.

Follow-up verification after the documentation push on 2026-10-15:

- Run `38803716931` completed against head SHA
  `066037920817b5f616beadb7103db5b20556329d`.
- `build (Debug)`, `build (Release)` and `sanitizers` all passed. The sanitizer
  job built with ASan/UBSan and its test step passed.
- `cross-builds (web, false)` failed during `Configure and Build MUSL
  (Static)` with undefined LLVM linker symbols, including `IRBuilderBase`,
  `TargetRegistry`, and `std::__1`-based signatures. This is a MUSL
  cross-link failure, not a sanitizer failure.
- `cross-builds (wasm-runtime, true)` was cancelled after the web matrix job
  failed. Its MUSL build was cancelled and its later WebAssembly checks were
  skipped. Do not record this run as evidence that the wasm-runtime MUSL
  build independently failed.
- The overall run conclusion was `failure`. The Debug, Release and sanitizer
  successes do not make the full workflow green.

The older run `38767587346` tested SHA
`5aaa899dac6f9ca402c1c35399037fabfc396331`, before the sanitizer workflow
commit. Its Debug and Release jobs passed, and its MUSL cross-build failed
with undefined LLVM C++ symbols. It is historical context, not sanitizer
validation.

Success criteria: DONE. A completed run for the remediation head SHA reports
`sanitizers: success`, the branch-protection API result is recorded, and this
verification record is kept separately from the implementation commits.

## Final Acceptance

After both steps are implemented:

1. Confirm step 1 remains in commit `266d2147`. Do not amend that commit.
2. Run `cmake --build build --target test-frontend -j4`.
3. Run `ctest --test-dir build --output-on-failure`.
4. Run the sanitizer configure, build and CTest commands. The recorded local
   run used Clang 21 because Clang 18 was unavailable. The workflow's exact
   Clang 18 commands remain in `.github/workflows/ci.yml`.
5. Run `python3 scripts/check-docs-consistency.py --root /home/diogo/Zith`.
6. Validate `.github/workflows/ci.yml` with `actionlint` when available.
   Otherwise parse it with an installed YAML parser and inspect the job's
   trigger, compiler, flags and failure behavior.
7. Run `git diff --check` and inspect the complete diff. Confirm unrelated
   pre-existing changes remain untouched.

Local acceptance requires the focused frontend test, both recorded full CTest
suites, documentation consistency, workflow validation and a clean diff check.
Remote sanitizer acceptance passed in runs `38798317687` and `38803716931`.
The latest overall workflow is not green: its web MUSL cross-build failed at
link time and the wasm-runtime matrix job was cancelled before its later
checks. The recorded branch-protection query on 2026-10-15 found `main`
unprotected, so the sanitizer job was not enforced by a required check at
that snapshot.
