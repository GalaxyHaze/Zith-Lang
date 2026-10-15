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
- The remote GitHub Actions result and branch-protection required-check setting
  have not been verified. The workflow definition alone does not make a job a
  required branch-protection check.
- `scripts/check-docs-consistency.py` passed and Ruby's standard YAML parser
  accepted the workflow. `actionlint` is not installed in the local environment.
- `cpp_tidy` reported no findings in the changed C++ files. `cpp_check`
  reported warnings in existing, unrelated project code.

For the CI configure, build and CTest commands, see
[`codebase-quality-remediation.md`](../docs/plans/codebase-quality-remediation.md).
