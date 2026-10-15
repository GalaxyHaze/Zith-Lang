# Parser Depth Limit

`AstLowerer` limits active recursive parser entries to 128. The shared RAII
guard covers expressions, types, blocks, and conditional tails. Overflow emits
`E1010`, returns an error node and scans iteratively to a delimiter or
synchronization point. The frontend tests cover both sides of the boundary and
verify that later declarations survive recovery. The change landed separately
in commit `266d2147`.

## Validation Notes

- The focused frontend test and both full CTest suites passed in the
  post-audit validation. The normal and ASan/UBSan suites each passed 42/42.
- The local sanitizer build used Clang 21, not Clang 18. Clang 18 is configured
  in CI but was not installed in this environment.
- A fresh `cpp_sanitize` project build selected `/usr/bin/cc` for C and could
  not link because GCC's ASan/UBSan runtimes are unavailable here, even when
  `compiler="clang++"` was passed. The successful local validation instead
  used a fresh CMake build with both C and C++ compilers explicitly set to
  Clang.
- LeakSanitizer cannot run under local `ptrace`, so local ASan/UBSan CTest used
  `ASAN_OPTIONS=detect_leaks=0`. This does not establish leak cleanliness.
  Keep leak detection enabled in CI.

The audit at
[`../docs/codebase-audit-2026-10-15.md`](../docs/codebase-audit-2026-10-15.md)
predates this validation. Its statement that tests and sanitizers were not run
describes the audit snapshot and is not the current validation status.
