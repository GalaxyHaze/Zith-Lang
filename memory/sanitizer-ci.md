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
- On 2026-10-15, run `38798317687` tested remediation head SHA
  `29366f79691df1d4a20e2f23e3d82cfe890715a2`. The `sanitizers` job passed:
  the Clang 18 ASan/UBSan build succeeded and all 42 CTest tests passed with
  zero failures. The Debug and Release jobs also passed.
- The overall run conclusion was `failure`. The `cross-builds (web, false)`
  job failed and the MUSL step in `cross-builds (wasm-runtime, true)` failed,
  both with undefined LLVM C++ link symbols. The wasm-runtime job was
  cancelled after its MUSL step failed. This is separate from sanitizer
  validation; do not describe the workflow run as fully green.
- Follow-up push run `38803716931` tested SHA
  `066037920817b5f616beadb7103db5b20556329d`. Debug, Release and the
  ASan/UBSan `sanitizers` job passed. `cross-builds (web, false)` failed in
  `Configure and Build MUSL (Static)` with undefined LLVM linker symbols,
  including `IRBuilderBase`, `TargetRegistry` and signatures using
  `std::__1`. `cross-builds (wasm-runtime, true)` was cancelled after the
  web matrix job failed. Its MUSL step was cancelled and later WebAssembly
  checks were skipped. Attribute the failure to the MUSL cross-link, not
  sanitizer validation, and do not claim the wasm-runtime build independently
  failed in this run. The overall workflow conclusion was `failure`.
- The required-status-checks API returned `404 Branch not protected` for
  `main` on 2026-10-15. No required status checks were configured at that
  snapshot, so the sanitizer job was not enforced by branch protection.
- The older run `38767587346` tested earlier SHA
  `5aaa899dac6f9ca402c1c35399037fabfc396331`. Debug and Release passed, while
  `cross-builds (wasm-runtime)` failed during `Configure and Build MUSL
  (Static)` with undefined C++ symbols from LLVM archives. That run predates
  the sanitizer job and is historical context, not sanitizer validation.
- `scripts/check-docs-consistency.py` passed and Ruby's standard YAML parser
  accepted the workflow. `actionlint` is not installed in the local environment.
- `cpp_tidy` reported no findings in the changed C++ files. `cpp_check`
  reported warnings in existing, unrelated project code.

For the CI configure, build and CTest commands, see
[`codebase-quality-remediation.md`](../docs/plans/codebase-quality-remediation.md).

## MUSL LLVM Link Failure

Run `38803716931` compiled `libzithcLib.a` but failed while linking `zithc`
in `cross-builds (web, false)`. Its log has undefined references with
`std::__cxx11` from LLVM archive members under `/usr/lib/llvm-18/lib`, while
Zith objects reference LLVM APIs such as `IRBuilderBase` and `TargetRegistry`
with `std::__1` signatures. The workflow uses Zig `0.13.0`, target
`x86_64-linux-musl`, and static linking against the host LLVM 18 package.

This is consistent with a C++ ABI mismatch, but does not prove one: the
effective Zig C++ headers/runtime and symbol definitions in the archives have
not been inspected. The local environment did not have Zig `0.13.0`, and no
reproduction or archive inspection was performed. Do not treat this as a
confirmed root cause or as a separate WASM failure; the
`cross-builds (wasm-runtime, true)` job was cancelled before its MUSL build and
WebAssembly checks.

To establish causality, reproduce the same CI build, capture the final link
command and effective C++ headers/libraries with `zig c++ -###`, compare
undefined and defined symbols with `llvm-nm -C`, then run a control build with
the C++ ABI aligned to the LLVM installation. Keep the ABI explanation marked
as a hypothesis unless that control resolves the symbols and links.
See [`musl-llvm-link-failure.md`](../docs/research/musl-llvm-link-failure.md)
for the investigation record and CI log.
