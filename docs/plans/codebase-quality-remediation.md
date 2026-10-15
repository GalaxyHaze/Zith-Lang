# Codebase Quality Remediation Plan

Objective: bound recursive parser work and add memory/undefined-behavior
sanitizer coverage for the compiler tests, following
[`codebase-audit-2026-10-15.md`](../codebase-audit-2026-10-15.md).

Status: steps 1 and 2 are implemented and locally verified. Step 3 is pending:
the Clang 18 sanitizer job has not run against these commits. On 2026-10-15,
GitHub reported `main` as unprotected, and local `main` was two commits ahead
of `origin/main`.

Preconditions:

- Work from `/home/diogo/Zith`.
- Preserve pre-existing modifications. The repository may have a dirty
  worktree.
- Keep step 1 in commit `266d2147` unchanged. Make step 2 a separate commit.
- Do not push commits to `main` without explicit user authorization.
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

### 2. Add an ASan/UBSan CI run (IMPLEMENTED, locally verified)

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

Success criteria: local implementation acceptance is DONE. Remote acceptance
remains pending until the GitHub sanitizer job passes with Clang 18. Whether
branch protection requires this job is a separate repository-host setting.
The 2026-10-15 GitHub API query returned `404 Branch not protected`, so
`main` did not have required status checks configured at that time.

### 3. Verify the remote sanitizer job (PENDING PUBLICATION)

Goal: the Clang 18 sanitizer workflow has a successful GitHub Actions run for
the remediation commits, and the branch-protection state is recorded.

Sub-steps:

1. From `/home/diogo/Zith`, run `git status -sb` and
   `git log -2 --oneline`. Confirm the intended remediation commits are
   `266d2147` and `b6e55e97` and identify whether they are still ahead of
   `origin/main`.
2. Do not push while user authorization is absent. Ask for explicit
   authorization to publish the commits to `main`. If authorization is denied
   or not given, leave this step pending and do not claim remote validation.
3. After authorization, run `git push origin main`. Stop if the push is
   rejected. Do not force-push, rebase shared history, or bypass branch rules.
4. Record the local tip with `git rev-parse HEAD`. List workflow runs with
   `gh run list --repo GalaxyHaze/Zith-Lang --workflow ci.yml --limit 10 --json databaseId,headSha,status,conclusion,createdAt,event,displayTitle`.
   Select the run whose `headSha` equals the published tip SHA. If a later
   commit is pushed first, stop and identify the run for the correct tip.
5. Inspect that run with
   `gh run view <run-id> --repo GalaxyHaze/Zith-Lang --json headSha,status,conclusion,jobs`.
   Confirm that the `sanitizers` job conclusion is `success`.
6. If `sanitizers` fails, retrieve its failed-step log with
   `gh run view <run-id> --repo GalaxyHaze/Zith-Lang --log-failed`. Fix the
   sanitizer or test defect in a separate commit, push only after the same
   authorization, and inspect the new run.
7. Query
   `gh api repos/GalaxyHaze/Zith-Lang/branches/main/protection/required_status_checks`.
   Record the returned required contexts. Treat HTTP 404 with `Branch not
   protected` as evidence that no required status checks are configured.
8. Update this plan and `memory/sanitizer-ci.md` with the run ID, head SHA,
   sanitizer conclusion and observed branch-protection state. Run
   `python3 scripts/check-docs-consistency.py --root /home/diogo/Zith`,
   validate the workflow YAML, run `git diff --check`, and commit the
   verification record separately.

Expected output: a GitHub Actions run for the remediation head SHA has a
`sanitizers` job with conclusion `success`. The branch-protection API returns
the required contexts or the explicit unprotected-branch response.

Failure checks:

- If the newest run predates `b6e55e97`, it does not verify this workflow
  change. Publish the commits only after explicit authorization, then inspect
  the resulting run.
- If the run is still in progress, poll that run ID until it is completed.
  Do not report a pending job as passed or failed.
- If `sanitizers` fails, preserve the failing run and logs, fix the reported
  defect, and create a separate fix commit. Do not weaken sanitizer flags or
  mark the job as allowed to fail.
- If another job fails while `sanitizers` passes, record both conclusions
  separately. Do not attribute a failure in an unrelated job to the
  sanitizer change.
- If the branch-protection API returns 403 or another error instead of the
  documented unprotected response, report branch protection as unverified.
  Do not infer that it is enabled or disabled.

Recorded remote snapshot on 2026-10-15: run `38767587346` tested head SHA
`5aaa899dac6f9ca402c1c35399037fabfc396331`, before the sanitizer workflow
commit. Its Debug and Release jobs passed. `cross-builds (wasm-runtime)` failed
in `Configure and Build MUSL (Static)` with undefined C++ symbols from LLVM
archives. That run does not verify the new sanitizer job. The required-status
checks endpoint returned `404 Branch not protected`. The remediation commits
were still local and had not been tested remotely.

Success criteria: DONE only when a completed run for a remediation head SHA
reports `sanitizers: success`, the branch-protection API result is recorded,
and the verification record is committed separately.

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
Overall CI acceptance remains open until step 3 succeeds. The current recorded
state is that `main` is unprotected, so the sanitizer job is not enforced by a
required branch-protection check.
