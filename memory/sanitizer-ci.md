# Sanitizer CI

The `sanitizers` GitHub Actions job configures a fresh Debug build with Clang
18, LLVM required, AddressSanitizer and UndefinedBehaviorSanitizer, then builds
all targets and runs the complete CTest suite. Keep the job failing on build,
test or sanitizer errors.

## Verification State

- Local regular CTest passed 42/42.
- Local ASan/UBSan CTest passed 42/42 in a fresh Clang 21 build. Leak detection
  was disabled locally with `ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer
  cannot run under this environment's `ptrace` setup. This is not a leak check.
- Clang 18 is configured in the workflow but was not installed locally.
- The remediation commits are local and have not yet produced a remote
  sanitizer run. Do not treat older workflow runs as validation of this job.
- On 2026-10-15, run `38767587346` tested earlier SHA
  `5aaa899dac6f9ca402c1c35399037fabfc396331`. Debug and Release passed, while
  `cross-builds (wasm-runtime)` failed during `Configure and Build MUSL
  (Static)` with undefined C++ symbols from LLVM archives. That run predates
  the sanitizer job and does not validate it.
- The required-status-checks API returned `404 Branch not protected` for
  `main` on 2026-10-15. The sanitizer job is therefore not enforced by branch
  protection at that snapshot.
- Publish commits to `main` only after explicit user authorization.
- `scripts/check-docs-consistency.py` passed and Ruby's standard YAML parser
  accepted the workflow. `actionlint` is not installed in the local environment.
- `cpp_tidy` reported no findings in the changed C++ files. `cpp_check`
  reported warnings in existing, unrelated project code.

For the CI configure, build and CTest commands, see
[`codebase-quality-remediation.md`](../docs/plans/codebase-quality-remediation.md).
