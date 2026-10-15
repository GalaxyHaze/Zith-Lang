# Parser Depth Limit

`AstLowerer` limits active recursive parser entries to 128. The shared RAII
guard covers expressions, types, blocks, and conditional tails; overflow emits
`E1010`, returns an error node, and scans iteratively to a delimiter or
synchronization point. The frontend tests pin both sides of the boundary and
check that later declarations survive recovery.

## Validation Notes

- The `cpp_sanitize` fresh project build selected `/usr/bin/cc` for C and could
  not link because this environment lacks GCC's ASan/UBSan runtimes, even when
  `compiler="clang++"` was passed. An isolated CMake build works when both C and
  C++ compilers are explicitly set to Clang.
- Local CTest under ASan/UBSan reported that LeakSanitizer cannot run under
  `ptrace`. The focused test passed with `ASAN_OPTIONS=detect_leaks=0`; keep
  leak detection enabled in CI, where this local execution limitation does
  not apply.
